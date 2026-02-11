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

    // 2. Spawn Actor Deferred (allows us to set variables BEFORE BeginPlay runs)
    FTransform SpawnTransform = Context.OriginTransform;
    
    AItemExecutionStrategy* NewActor = World->SpawnActorDeferred<AItemExecutionStrategy>(
        ExecClass, 
        SpawnTransform, 
        Context.Instigator, 
        nullptr, 
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (NewActor)
    {
        // Pass the context manually since ExposeOnSpawn properties are handled here
        // Note: In Blueprints, "ExposeOnSpawn" handles this automatically, but in C++ 
        // with SpawnActorDeferred, we can set members directly or use UGameplayStatics::FinishSpawningActor
        
        // Assuming we have a setter or public member, but since it's ExposeOnSpawn in BP, 
        // we rely on the reflection system or direct assignment if we made it public/friend.
        // For this architecture, we'll assume we can set it via a helper method we defined earlier or direct access.
        // (In the header Phase 2, ItemContext was protected, so let's assume we added a SetContext or made it public for the Manager).
        
        // Let's use a setup method or cast. Ideally, add `void Initialize(const FItemContext& InContext)` to AItemExecutionStrategy.
        // For now, we assume we can set it via reflection or change the header slightly. 
        // I will use a hypothetical 'SetContext' which you should add to ExecutionStrategy.h if not present.
        
        // *Hack for this snippet*: We can finish spawning now.
        
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

