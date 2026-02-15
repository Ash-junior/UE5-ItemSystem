#include "Core/ItemSpawnPoint.h"

#include "Core/InventoryComponent.h"
#include "Core/ItemInterface.h"
#include "Core/ItemSystemManager.h"
#include "Core/ItemSystemLog.h"
#include "Data/ItemDefinition.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"

AItemSpawnPoint::AItemSpawnPoint()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    RootComponent = SceneRoot;

    ItemPreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemPreviewMesh"));
    ItemPreviewMesh->SetupAttachment(SceneRoot);
    ItemPreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    ItemPreviewMesh->SetGenerateOverlapEvents(false);

    PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
    PickupTrigger->SetupAttachment(SceneRoot);
    PickupTrigger->SetSphereRadius(120.0f);
    PickupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PickupTrigger->SetGenerateOverlapEvents(true);
    PickupTrigger->OnComponentBeginOverlap.AddDynamic(this, &AItemSpawnPoint::HandlePickupTriggerBeginOverlap);
}

void AItemSpawnPoint::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (UItemSystemManager* Manager = UItemSystemManager::Get(this))
        {
            Manager->RegisterSpawnPoint(this);
        }
    }

    RefreshVisual();
}

void AItemSpawnPoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        if (UItemSystemManager* Manager = UItemSystemManager::Get(this))
        {
            Manager->UnregisterSpawnPoint(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

void AItemSpawnPoint::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AItemSpawnPoint, AssignedItem);
}

void AItemSpawnPoint::SetAssignedItem(UItemDefinition* NewItem)
{
    if (!HasAuthority())
    {
        return;
    }

    if (AssignedItem == NewItem)
    {
        return;
    }

    AssignedItem = NewItem;
    OnRep_AssignedItem();
}

void AItemSpawnPoint::ConsumeAssignedItem(bool bRequestImmediateRespawn)
{
    if (!HasAuthority())
    {
        return;
    }

    if (UItemSystemManager* Manager = UItemSystemManager::Get(this))
    {
        Manager->NotifySpawnPointItemConsumed(this, bRequestImmediateRespawn);
        return;
    }

    SetAssignedItem(nullptr);
}

bool AItemSpawnPoint::MatchesAdditionalFilter(const UItemDefinition* Item) const
{
    if (!Item)
    {
        return false;
    }

    if (AdditionalItemFilter.IsEmpty())
    {
        return true;
    }

    return AdditionalItemFilter.Matches(Item->IdentityTags);
}

void AItemSpawnPoint::OnRep_AssignedItem()
{
    RefreshVisual();
    OnAssignedItemChanged.Broadcast(this, AssignedItem);
}

void AItemSpawnPoint::HandlePickupTriggerBeginOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    if (!HasAuthority() || !AssignedItem || !OtherActor || OtherActor == this)
    {
        return;
    }

    AActor* RecipientActor = ResolveRecipientActor(OtherActor);
    UInventoryComponent* RecipientInventory = FindRecipientInventory(RecipientActor);
    if (!RecipientInventory)
    {
        if (IsItemSystemQAEnabled())
        {
            UE_LOG(LogItemSystem, Log, TEXT("QA: SpawnPoint %s overlap by %s but no valid inventory recipient found."),
                *GetName(),
                *GetNameSafe(OtherActor));
        }
        return;
    }

    RecipientInventory->Server_GrantItem(AssignedItem, FMath::Max(1, GrantAmount));
    OnItemGranted.Broadcast(this, RecipientActor, AssignedItem);

    if (bConsumeOnSuccessfulGrant)
    {
        ConsumeAssignedItem(bRequestImmediateRespawnOnConsume);
    }
}

AActor* AItemSpawnPoint::ResolveRecipientActor(AActor* OverlapActor) const
{
    if (!OverlapActor)
    {
        return nullptr;
    }

    auto ResolveOverlapActor = [this, OverlapActor]() -> AActor*
    {
        return FindRecipientInventory(OverlapActor) ? OverlapActor : nullptr;
    };

    switch (RecipientPolicy)
    {
    case EItemSpawnRecipientPolicy::OverlappingActorOnly:
        return ResolveOverlapActor();

    case EItemSpawnRecipientPolicy::OverlapActorIfHasTagElseDesignated:
        if (!OverlapReceivesItemTag.IsValid() || ActorHasGameplayTagForRouting(OverlapActor, OverlapReceivesItemTag))
        {
            if (AActor* OverlapRecipient = ResolveOverlapActor())
            {
                return OverlapRecipient;
            }
        }

        if (AActor* Designated = ResolveDesignatedRecipient(OverlapActor))
        {
            return Designated;
        }

        return bFallbackToOverlapIfDesignatedNotFound ? ResolveOverlapActor() : nullptr;

    case EItemSpawnRecipientPolicy::DesignatedActorOnly:
        if (AActor* Designated = ResolveDesignatedRecipient(OverlapActor))
        {
            return Designated;
        }

        return bFallbackToOverlapIfDesignatedNotFound ? ResolveOverlapActor() : nullptr;

    default:
        return ResolveOverlapActor();
    }
}

AActor* AItemSpawnPoint::ResolveDesignatedRecipient(AActor* OverlapActor) const
{
    if (DesignatedRecipientActor)
    {
        const bool bTagOk = !DesignatedRecipientTag.IsValid()
            || ActorHasGameplayTagForRouting(DesignatedRecipientActor, DesignatedRecipientTag);
        const bool bRelationOk = DoesActorMatchRecipientRelation(OverlapActor, DesignatedRecipientActor);
        if (bTagOk && bRelationOk && FindRecipientInventory(DesignatedRecipientActor))
        {
            return DesignatedRecipientActor;
        }
    }

    return FindTaggedRecipientInWorld(OverlapActor);
}

AActor* AItemSpawnPoint::FindTaggedRecipientInWorld(AActor* OverlapActor) const
{
    if (!DesignatedRecipientTag.IsValid())
    {
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    AActor* BestActor = nullptr;
    float BestDistanceSq = TNumericLimits<float>::Max();

    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Candidate = *It;
        if (!Candidate || Candidate == this)
        {
            continue;
        }

        if (!ActorHasGameplayTagForRouting(Candidate, DesignatedRecipientTag))
        {
            continue;
        }

        if (!DoesActorMatchRecipientRelation(OverlapActor, Candidate))
        {
            continue;
        }

        if (!FindRecipientInventory(Candidate))
        {
            continue;
        }

        const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
        if (DistSq < BestDistanceSq)
        {
            BestDistanceSq = DistSq;
            BestActor = Candidate;
        }
    }

    return BestActor;
}

bool AItemSpawnPoint::DoesActorMatchRecipientRelation(AActor* OverlapActor, AActor* CandidateActor) const
{
    if (!CandidateActor)
    {
        return false;
    }

    if (DesignatedRecipientRelation == EItemSpawnRecipientRelation::Any)
    {
        return true;
    }

    if (!OverlapActor || !OverlapActor->Implements<UItemInterface>() || !CandidateActor->Implements<UItemInterface>())
    {
        return false;
    }

    const int32 OverlapTeam = IItemInterface::Execute_GetTeamID(OverlapActor);
    const int32 CandidateTeam = IItemInterface::Execute_GetTeamID(CandidateActor);

    if (DesignatedRecipientRelation == EItemSpawnRecipientRelation::SameTeamAsOverlappingActor)
    {
        return OverlapTeam == CandidateTeam;
    }

    if (DesignatedRecipientRelation == EItemSpawnRecipientRelation::EnemyOfOverlappingActor)
    {
        return OverlapTeam != CandidateTeam;
    }

    return true;
}

bool AItemSpawnPoint::ActorHasGameplayTagForRouting(AActor* Actor, const FGameplayTag& Tag) const
{
    if (!Actor)
    {
        return false;
    }

    if (!Tag.IsValid())
    {
        return true;
    }

    if (!Actor->Implements<UItemInterface>())
    {
        return false;
    }

    return IItemInterface::Execute_HasGameplayTag(Actor, Tag);
}

UInventoryComponent* AItemSpawnPoint::FindRecipientInventory(AActor* CandidateActor) const
{
    return CandidateActor ? CandidateActor->FindComponentByClass<UInventoryComponent>() : nullptr;
}

void AItemSpawnPoint::RefreshVisual()
{
    if (!ItemPreviewMesh)
    {
        return;
    }

    UStaticMesh* Preview = nullptr;
    if (AssignedItem)
    {
        Preview = AssignedItem->Visuals.HeldMesh;
    }

    ItemPreviewMesh->SetStaticMesh(Preview);

    const bool bShowMesh = (Preview != nullptr) || (!bHidePreviewWhenNoItem);
    ItemPreviewMesh->SetHiddenInGame(!bShowMesh);
}
