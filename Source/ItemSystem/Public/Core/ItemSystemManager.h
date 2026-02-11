#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Core/ItemSystemTypes.h"
#include "ItemSystemManager.generated.h"

class UItemDefinition;
class AItemExecutionStrategy;
class UItemDistributionPolicy;

/**
 * The central manager for the Item System.
 * Usually attached to the GameState.
 * Responsibilities:
 * 1. Maintaining the registry of all available items.
 * 2. Handling item queries (filtering/selection).
 * 3. Spawning the physical execution actors (handling soft references and pooling).
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ITEMSYSTEM_API UItemSystemManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UItemSystemManager();

    // --- Static Accessor ---

    /**
     * Static helper to find the Manager in the current world.
     * Iterates GameState components to find itself.
     */
    UFUNCTION(BlueprintPure, Category = "Item System", meta = (WorldContext = "WorldContextObject"))
    static UItemSystemManager* Get(const UObject* WorldContextObject);

protected:
    // --- Data Registry ---

    // List of all items available in the current game mode.
    // Should be populated at Start (e.g., from the GameMode or a DataRegistry).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
    TArray<UItemDefinition*> GlobalItemRegistry;

    // Internal pool for actors (Class -> Array of Inactive Actors)
    // Used to avoid destroying/spawning frequently used projectiles.
    UPROPERTY()
    TMap<UClass*, FItemActorPool> ActorPools;

public:
    // --- Public API ---

    /**
     * Registers a list of items into the system (called by GameMode).
     */
    UFUNCTION(BlueprintCallable, Category = "Item System")
    void RegisterItems(const TArray<UItemDefinition*>& Items);

    /**
     * Returns a random item that matches the tag query.
     * Useful for Item Boxes/Pickups.
     * @param Query - The rules for selection (e.g. "Family.Offense AND NOT Rarity.Legendary")
     */
    UFUNCTION(BlueprintCallable, Category = "Item System")
    UItemDefinition* GetItemByQuery(FGameplayTagQuery Query) const;

    /**
     * Returns an item selected by a distribution policy.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System")
    UItemDefinition* GetItemByPolicy(UItemDistributionPolicy* Policy, AActor* Requester) const;

    /**
     * The Main Function: Transforms a Context into a real World Actor.
     * Handles loading the SoftClassPtr from the definition.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System")
    AItemExecutionStrategy* SpawnItemExecution(const FItemContext& Context);

    /**
     * Returns an actor to the pool for future reuse.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System")
    void ReleaseExecutionActor(AItemExecutionStrategy* Actor);

protected:
    
    // --- Internal Logic ---

    /**
     * Helper to load the class synchronously if not loaded.
     * In a production environment, this should ideally be async or preloaded.
     */
    UClass* ResolveExecutionClass(const TSoftClassPtr<AItemExecutionStrategy>& SoftClass);

    AItemExecutionStrategy* GetPooledActor(UClass* ExecClass);
    void AddToPool(AItemExecutionStrategy* Actor);
};
