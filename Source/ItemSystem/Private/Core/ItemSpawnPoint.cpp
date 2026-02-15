#include "Core/ItemSpawnPoint.h"

#include "Core/ItemSystemManager.h"
#include "Data/ItemDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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
