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
#include "Components/BillboardComponent.h"
#include "Components/TextRenderComponent.h"
#include "NiagaraComponent.h"
#include "EngineUtils.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ObjectKey.h"

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

    SpawnVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("SpawnVFXComponent"));
    SpawnVFXComponent->SetupAttachment(SceneRoot);
    SpawnVFXComponent->SetAutoActivate(true);

    PickupTrigger = CreateDefaultSubobject<USphereComponent>(TEXT("PickupTrigger"));
    PickupTrigger->SetupAttachment(SceneRoot);
    PickupTrigger->SetSphereRadius(120.0f);
    PickupTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupTrigger->SetCollisionObjectType(ECC_WorldDynamic);
    PickupTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PickupTrigger->SetGenerateOverlapEvents(true);
    PickupTrigger->OnComponentBeginOverlap.AddDynamic(this, &AItemSpawnPoint::HandlePickupTriggerBeginOverlap);

#if WITH_EDITORONLY_DATA
    EditorBillboard = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("EditorBillboard"));
    if (EditorBillboard)
    {
        EditorBillboard->SetupAttachment(SceneRoot);
        EditorBillboard->SetHiddenInGame(true);
        EditorBillboard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        EditorBillboard->SetIsVisualizationComponent(true);
        EditorBillboard->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
    }

    EditorLabel = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("EditorLabel"));
    if (EditorLabel)
    {
        EditorLabel->SetupAttachment(SceneRoot);
        EditorLabel->SetHiddenInGame(true);
        EditorLabel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        EditorLabel->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
        EditorLabel->SetWorldSize(24.0f);
        EditorLabel->SetTextRenderColor(FColor(255, 230, 80));
        EditorLabel->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
        EditorLabel->SetText(FText::FromString(TEXT("Item Spawn")));
    }
#endif
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

void AItemSpawnPoint::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
    if (EditorLabel)
    {
        EditorLabel->SetText(FText::FromString(GetActorNameOrLabel()));
    }
#endif
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
    bPendingConsume = false; // Release lock: item has been committed (cleared or replaced)
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

void AItemSpawnPoint::ApplyPickupRoutingSettings(const FItemSpawnPointPickupRoutingSettings& NewSettings)
{
    if (!HasAuthority())
    {
        return;
    }

    PickupRoutingSettings = NewSettings;
    PickupRoutingSettings.OverrideGrantAmount = FMath::Max(1, PickupRoutingSettings.OverrideGrantAmount);
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
    if (!HasAuthority() || !OtherActor || OtherActor == this)
    {
        return;
    }

    if (!AssignedItem || bPendingConsume)
    {
        if (bPendingConsume && IsItemSystemQAEnabled())
        {
            UE_LOG(LogItemSystem, Log, TEXT("QA: SpawnPoint %s blocked duplicate overlap from %s (consume pending)."),
                *GetName(), *GetNameSafe(OtherActor));
        }
        return;
    }

    if (PickupRoutingSettings.PickupMethod != EItemSpawnPickupMethod::TriggerOverlap)
    {
        return;
    }

    const float Now = GetWorld()->GetTimeSeconds();

    // Spawn-point-wide cooldown: block all actors until the delay has elapsed.
    if (PickupCooldownSeconds > 0.0f && LastPickupWorldTime >= 0.0f)
    {
        const float Remaining = PickupCooldownSeconds - (Now - LastPickupWorldTime);
        if (Remaining > 0.0f)
        {
            if (IsItemSystemQAEnabled())
            {
                UE_LOG(LogItemSystem, Log, TEXT("QA: SpawnPoint %s pickup cooldown blocked %s (%.1fs remaining)."),
                    *GetName(), *GetNameSafe(OtherActor), Remaining);
            }
            return;
        }
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

    // Per-actor cooldown: block this specific recipient until their individual delay has elapsed.
    if (PerActorPickupCooldownSeconds > 0.0f)
    {
        if (const float* LastTime = ActorLastPickupTimes.Find(FObjectKey(RecipientActor)))
        {
            const float Remaining = PerActorPickupCooldownSeconds - (Now - *LastTime);
            if (Remaining > 0.0f)
            {
                if (IsItemSystemQAEnabled())
                {
                    UE_LOG(LogItemSystem, Log, TEXT("QA: SpawnPoint %s per-actor cooldown blocked %s (%.1fs remaining)."),
                        *GetName(), *GetNameSafe(RecipientActor), Remaining);
                }
                return;
            }
        }
    }

    // Lock synchronously before the grant so any further overlap in the same frame is blocked.
    if (PickupRoutingSettings.bConsumeOnSuccessfulGrant)
    {
        bPendingConsume = true;
    }

    // Record timestamps before the grant so cooldowns are enforced even if the grant fails silently.
    if (PickupCooldownSeconds > 0.0f)
    {
        LastPickupWorldTime = Now;
    }
    if (PerActorPickupCooldownSeconds > 0.0f)
    {
        ActorLastPickupTimes.FindOrAdd(FObjectKey(RecipientActor)) = Now;
    }

    RecipientInventory->Server_GrantItem(AssignedItem, ResolvePickupGrantAmount());
    OnItemGranted.Broadcast(this, RecipientActor, AssignedItem);

    if (PickupRoutingSettings.bConsumeOnSuccessfulGrant)
    {
        ConsumeAssignedItem(PickupRoutingSettings.bRequestImmediateRespawnOnConsume);
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

    switch (PickupRoutingSettings.RecipientPolicy)
    {
    case EItemSpawnRecipientPolicy::OverlappingActorOnly:
        return ResolveOverlapActor();

    case EItemSpawnRecipientPolicy::OverlapActorIfHasTagElseDesignated:
        if (!PickupRoutingSettings.OverlapReceivesItemTag.IsValid()
            || ActorHasGameplayTagForRouting(OverlapActor, PickupRoutingSettings.OverlapReceivesItemTag))
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

        return PickupRoutingSettings.bFallbackToOverlapIfDesignatedNotFound ? ResolveOverlapActor() : nullptr;

    case EItemSpawnRecipientPolicy::DesignatedActorOnly:
        if (AActor* Designated = ResolveDesignatedRecipient(OverlapActor))
        {
            return Designated;
        }

        return PickupRoutingSettings.bFallbackToOverlapIfDesignatedNotFound ? ResolveOverlapActor() : nullptr;

    default:
        return ResolveOverlapActor();
    }
}

int32 AItemSpawnPoint::ResolvePickupGrantAmount() const
{
    int32 ResolvedAmount = 1;

    if (AssignedItem)
    {
        ResolvedAmount = FMath::Max(1, AssignedItem->PickupGrantAmount);
    }

    if (PickupRoutingSettings.bOverrideItemGrantAmount)
    {
        ResolvedAmount = FMath::Max(1, PickupRoutingSettings.OverrideGrantAmount);
    }

    if (bUseLocalGrantAmountOverride)
    {
        ResolvedAmount = FMath::Max(1, LocalGrantAmountOverride);
    }

    return ResolvedAmount;
}

AActor* AItemSpawnPoint::ResolveDesignatedRecipient(AActor* OverlapActor) const
{
    if (PickupRoutingSettings.DesignatedRecipientActor)
    {
        const bool bTagOk = !PickupRoutingSettings.DesignatedRecipientTag.IsValid()
            || ActorHasGameplayTagForRouting(
                PickupRoutingSettings.DesignatedRecipientActor,
                PickupRoutingSettings.DesignatedRecipientTag);
        const bool bRelationOk = DoesActorMatchRecipientRelation(OverlapActor, PickupRoutingSettings.DesignatedRecipientActor);
        if (bTagOk && bRelationOk && FindRecipientInventory(PickupRoutingSettings.DesignatedRecipientActor))
        {
            return PickupRoutingSettings.DesignatedRecipientActor;
        }
    }

    return FindTaggedRecipientInWorld(OverlapActor);
}

AActor* AItemSpawnPoint::FindTaggedRecipientInWorld(AActor* OverlapActor) const
{
    if (!PickupRoutingSettings.DesignatedRecipientTag.IsValid())
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

        if (!ActorHasGameplayTagForRouting(Candidate, PickupRoutingSettings.DesignatedRecipientTag))
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

    if (PickupRoutingSettings.DesignatedRecipientRelation == EItemSpawnRecipientRelation::Any)
    {
        return true;
    }

    if (!OverlapActor || !OverlapActor->Implements<UItemInterface>() || !CandidateActor->Implements<UItemInterface>())
    {
        return false;
    }

    const int32 OverlapTeam = IItemInterface::Execute_GetTeamID(OverlapActor);
    const int32 CandidateTeam = IItemInterface::Execute_GetTeamID(CandidateActor);

    if (PickupRoutingSettings.DesignatedRecipientRelation == EItemSpawnRecipientRelation::SameTeamAsOverlappingActor)
    {
        return OverlapTeam == CandidateTeam;
    }

    if (PickupRoutingSettings.DesignatedRecipientRelation == EItemSpawnRecipientRelation::EnemyOfOverlappingActor)
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
