#include "Core/ItemSystemManager.h"
#include "Data/ItemDefinition.h"
#include "Strategies/ExecutionStrategy.h"
#include "Distribution/ItemDistributionPolicy.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameStateBase.h"

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

    AActor* Actor = Pool->InactiveActors.Pop(false);
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

