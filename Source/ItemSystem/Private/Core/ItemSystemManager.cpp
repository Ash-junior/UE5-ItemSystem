#include "Core/ItemSystemManager.h"
#include "Data/ItemDefinition.h"
#include "Strategies/ExecutionStrategy.h"
#include "Distribution/ItemDistributionPolicy.h"
#include "Core/ItemSpawnPoint.h"
#include "Core/ItemSystemLog.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"

UItemSystemManager::UItemSystemManager()
{
    PrimaryComponentTick.bCanEverTick = false;
}

UItemSystemManager* UItemSystemManager::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    AGameStateBase* GameState = World->GetGameState();
    if (!GameState) return nullptr;

    // Find the component on the GameState
    return GameState->FindComponentByClass<UItemSystemManager>();
}

void UItemSystemManager::RegisterItems(const TArray<UItemDefinition*>& Items)
{
    GlobalItemRegistry = Items;
    // Here we could build lookup maps for faster querying later
}

void UItemSystemManager::BeginPlay()
{
    Super::BeginPlay();

    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !bEnableWorldSpawnManagement)
    {
        return;
    }

    if (bAutoDiscoverSpawnPoints)
    {
        DiscoverSpawnPoints();
    }

    // Initial assignment does not consume limited reset budget.
    RefreshAllSpawnPoints(false);
    bWorldSpawnInitializationDone = true;
    StartWorldSpawnRefreshTimer();
}

void UItemSystemManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopWorldSpawnRefreshTimer();
    bWorldSpawnInitializationDone = false;
    Super::EndPlay(EndPlayReason);
}

UItemDefinition* UItemSystemManager::GetItemByQuery(FGameplayTagQuery Query) const
{
    if (GlobalItemRegistry.Num() == 0) return nullptr;

    TArray<UItemDefinition*> MatchingItems;

    // Filter items
    for (UItemDefinition* Item : GlobalItemRegistry)
    {
        if (Item && Query.Matches(Item->IdentityTags))
        {
            MatchingItems.Add(Item);
        }
    }

    // Return random match
    if (MatchingItems.Num() > 0)
    {
        int32 Index = FMath::RandRange(0, MatchingItems.Num() - 1);
        return MatchingItems[Index];
    }

    return nullptr;
}

UItemDefinition* UItemSystemManager::GetItemByPolicy(UItemDistributionPolicy* Policy, AActor* Requester) const
{
    if (!Policy || GlobalItemRegistry.Num() == 0)
    {
        return nullptr;
    }

    return Policy->SelectItem(Requester, GlobalItemRegistry);
}

AItemExecutionStrategy* UItemSystemManager::SpawnItemExecution(const FItemContext& Context)
{
    if (!Context.ItemDefinition) return nullptr;

    // 1. Resolve the Class (Load Soft Reference)
    UClass* ExecClass = ResolveExecutionClass(Context.ItemDefinition->ExecutionClass);
    if (!ExecClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("ItemSystem: Failed to load ExecutionClass for item %s"), *Context.ItemDefinition->GetName());
        return nullptr;
    }

    UWorld* World = GetWorld();
    if (!World) return nullptr;

    // 2. Try pooled actor first
    AItemExecutionStrategy* NewActor = GetPooledActor(ExecClass);
    const FTransform SpawnTransform = Context.OriginTransform;

    if (NewActor)
    {
        NewActor->SetItemContext(Context);
        NewActor->SetActorTransform(SpawnTransform);
        NewActor->SetActorHiddenInGame(false);
        NewActor->SetActorEnableCollision(true);
        NewActor->SetActorTickEnabled(true);
        NewActor->ResetForReuse();
        return NewActor;
    }

    // 3. Spawn Actor Deferred (allows us to set variables BEFORE BeginPlay runs)
    NewActor = World->SpawnActorDeferred<AItemExecutionStrategy>(
        ExecClass,
        SpawnTransform,
        Context.Instigator,
        nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (NewActor)
    {
        NewActor->SetItemContext(Context);
        UGameplayStatics::FinishSpawningActor(NewActor, SpawnTransform);
    }

    return NewActor;
}

UClass* UItemSystemManager::ResolveExecutionClass(const TSoftClassPtr<AItemExecutionStrategy>& SoftClass)
{
    if (SoftClass.IsNull()) return nullptr;

    // If already loaded, return it
    if (UClass* HardClass = SoftClass.Get())
    {
        return HardClass;
    }

    // Synchronous Load (Simple but causes hitch if asset is huge)
    return SoftClass.LoadSynchronous();
}

AItemExecutionStrategy* UItemSystemManager::GetPooledActor(UClass* ExecClass)
{
    if (!ExecClass)
    {
        return nullptr;
    }

    FItemActorPool* Pool = ActorPools.Find(ExecClass);
    if (!Pool || Pool->InactiveActors.Num() == 0)
    {
        return nullptr;
    }

    AActor* Actor = Pool->InactiveActors.Pop(EAllowShrinking::No);
    if (IsItemSystemQAEnabled() && Actor)
    {
        UE_LOG(LogItemSystem, Log, TEXT("QA: Reusing pooled actor %s"), *Actor->GetName());
    }
    return Cast<AItemExecutionStrategy>(Actor);
}

void UItemSystemManager::AddToPool(AItemExecutionStrategy* Actor)
{
    if (!Actor)
    {
        return;
    }

    FItemActorPool& Pool = ActorPools.FindOrAdd(Actor->GetClass());
    Pool.InactiveActors.Add(Actor);
    if (IsItemSystemQAEnabled())
    {
        UE_LOG(LogItemSystem, Log, TEXT("QA: Added actor to pool %s"), *Actor->GetName());
    }
}

void UItemSystemManager::ReleaseExecutionActor(AItemExecutionStrategy* Actor)
{
    if (!Actor)
    {
        return;
    }

    // Disable and hide
    Actor->SetActorEnableCollision(false);
    Actor->SetActorHiddenInGame(true);
    Actor->SetActorTickEnabled(false);

    AddToPool(Actor);
}

void UItemSystemManager::RegisterSpawnPoint(AItemSpawnPoint* SpawnPoint)
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !SpawnPoint || !bEnableWorldSpawnManagement)
    {
        return;
    }

    RegisteredSpawnPoints.AddUnique(SpawnPoint);

    if (bWorldSpawnInitializationDone && !SpawnPoint->HasAssignedItem())
    {
        RefreshSpawnPoint(SpawnPoint);
    }
}

void UItemSystemManager::UnregisterSpawnPoint(AItemSpawnPoint* SpawnPoint)
{
    if (!SpawnPoint)
    {
        return;
    }

    RegisteredSpawnPoints.RemoveSingleSwap(SpawnPoint);
}

void UItemSystemManager::RefreshAllSpawnPoints(bool bConsumeResetBudget)
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !bEnableWorldSpawnManagement)
    {
        return;
    }

    CleanupInvalidSpawnPoints();

    for (AItemSpawnPoint* SpawnPoint : RegisteredSpawnPoints)
    {
        RefreshSpawnPoint(SpawnPoint);
    }

    if (bConsumeResetBudget && WorldSpawnRefreshMode == EItemSpawnRefreshMode::TimedLimitedResets)
    {
        ++WorldSpawnResetsDone;
    }
}

void UItemSystemManager::RefreshSpawnPoint(AItemSpawnPoint* SpawnPoint)
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !bEnableWorldSpawnManagement || !SpawnPoint)
    {
        return;
    }

    UItemDefinition* SelectedItem = SelectItemForSpawnPoint(SpawnPoint);
    SpawnPoint->SetAssignedItem(SelectedItem);
}

void UItemSystemManager::NotifySpawnPointItemConsumed(AItemSpawnPoint* SpawnPoint, bool bRequestImmediateRespawn)
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !SpawnPoint || !bEnableWorldSpawnManagement)
    {
        return;
    }

    SpawnPoint->SetAssignedItem(nullptr);

    const bool bShouldRespawnNow = bRequestImmediateRespawn || bRefreshOnConsume;
    if (bShouldRespawnNow)
    {
        RefreshSpawnPoint(SpawnPoint);
    }
}

int32 UItemSystemManager::GetWorldSpawnResetsRemaining() const
{
    if (WorldSpawnRefreshMode != EItemSpawnRefreshMode::TimedLimitedResets)
    {
        return -1;
    }

    return FMath::Max(0, WorldSpawnMaxResets - WorldSpawnResetsDone);
}

void UItemSystemManager::DiscoverSpawnPoints()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    for (TActorIterator<AItemSpawnPoint> It(World); It; ++It)
    {
        RegisterSpawnPoint(*It);
    }
}

void UItemSystemManager::CleanupInvalidSpawnPoints()
{
    RegisteredSpawnPoints.RemoveAll([](const TObjectPtr<AItemSpawnPoint>& SpawnPoint)
    {
        return !IsValid(SpawnPoint);
    });
}

void UItemSystemManager::StartWorldSpawnRefreshTimer()
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !bEnableWorldSpawnManagement)
    {
        return;
    }

    StopWorldSpawnRefreshTimer();

    if (WorldSpawnRefreshMode == EItemSpawnRefreshMode::None)
    {
        return;
    }

    if (WorldSpawnRefreshInterval <= 0.0f)
    {
        UE_LOG(LogItemSystem, Warning, TEXT("WorldSpawnRefreshInterval must be > 0 to enable timed refresh."));
        return;
    }

    if (WorldSpawnRefreshMode == EItemSpawnRefreshMode::TimedLimitedResets && WorldSpawnMaxResets <= 0)
    {
        if (IsItemSystemQAEnabled())
        {
            UE_LOG(LogItemSystem, Log, TEXT("QA: Timed limited spawn refresh is disabled because WorldSpawnMaxResets is 0."));
        }
        return;
    }

    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().SetTimer(
            WorldSpawnRefreshTimerHandle,
            this,
            &UItemSystemManager::HandleWorldSpawnRefreshTick,
            WorldSpawnRefreshInterval,
            true
        );
    }
}

void UItemSystemManager::StopWorldSpawnRefreshTimer()
{
    UWorld* World = GetWorld();
    if (World)
    {
        World->GetTimerManager().ClearTimer(WorldSpawnRefreshTimerHandle);
    }
}

void UItemSystemManager::HandleWorldSpawnRefreshTick()
{
    const AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->HasAuthority() || !bEnableWorldSpawnManagement)
    {
        StopWorldSpawnRefreshTimer();
        return;
    }

    if (WorldSpawnRefreshMode == EItemSpawnRefreshMode::TimedInfinite)
    {
        RefreshAllSpawnPoints(false);
        return;
    }

    if (WorldSpawnRefreshMode == EItemSpawnRefreshMode::TimedLimitedResets)
    {
        if (WorldSpawnResetsDone >= WorldSpawnMaxResets)
        {
            StopWorldSpawnRefreshTimer();
            return;
        }

        RefreshAllSpawnPoints(true);

        if (WorldSpawnResetsDone >= WorldSpawnMaxResets)
        {
            StopWorldSpawnRefreshTimer();
        }
        return;
    }

    StopWorldSpawnRefreshTimer();
}

void UItemSystemManager::BuildWorldSpawnCandidateList(TArray<UItemDefinition*>& OutCandidates) const
{
    OutCandidates.Reset();
    OutCandidates.Reserve(GlobalItemRegistry.Num());

    for (UItemDefinition* Item : GlobalItemRegistry)
    {
        if (!Item)
        {
            continue;
        }

        if (!GlobalWorldSpawnFilter.IsEmpty() && !GlobalWorldSpawnFilter.Matches(Item->IdentityTags))
        {
            continue;
        }

        OutCandidates.Add(Item);
    }
}

UItemDefinition* UItemSystemManager::SelectItemForSpawnPoint(const AItemSpawnPoint* SpawnPoint) const
{
    if (!SpawnPoint)
    {
        return nullptr;
    }

    TArray<UItemDefinition*> BaseCandidates;
    BuildWorldSpawnCandidateList(BaseCandidates);
    if (BaseCandidates.Num() == 0)
    {
        return nullptr;
    }

    const UItemDistributionPolicy* Policy = SpawnPoint->GetDistributionPolicyOverride();
    if (!Policy)
    {
        Policy = WorldSpawnDistributionPolicy;
    }

    auto SelectFromList = [Policy, SpawnPoint](const TArray<UItemDefinition*>& Items) -> UItemDefinition*
    {
        if (Items.Num() == 0)
        {
            return nullptr;
        }

        if (!Policy)
        {
            const int32 Index = FMath::RandRange(0, Items.Num() - 1);
            return Items[Index];
        }

        return Policy->SelectItem(const_cast<AItemSpawnPoint*>(SpawnPoint), Items);
    };

    if (SpawnPoint->HasAdditionalFilter())
    {
        TArray<UItemDefinition*> FilteredCandidates;
        FilteredCandidates.Reserve(BaseCandidates.Num());

        for (UItemDefinition* Candidate : BaseCandidates)
        {
            if (SpawnPoint->MatchesAdditionalFilter(Candidate))
            {
                FilteredCandidates.Add(Candidate);
            }
        }

        UItemDefinition* Preferred = SelectFromList(FilteredCandidates);
        if (Preferred)
        {
            return Preferred;
        }

        if (SpawnPoint->GetConstraintMode() == EItemSpawnConstraintMode::StrictFilteredOnly)
        {
            if (IsItemSystemQAEnabled())
            {
                UE_LOG(LogItemSystem, Log, TEXT("QA: Strict filter prevented assignment on spawn point %s"), *SpawnPoint->GetName());
            }
            return nullptr;
        }
    }

    return SelectFromList(BaseCandidates);
}

