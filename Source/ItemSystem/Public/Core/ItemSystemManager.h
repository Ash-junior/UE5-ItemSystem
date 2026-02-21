#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "Core/ItemSystemTypes.h"
#include "Core/ItemSpawnPointRoutingTypes.h"
#include "ItemSystemManager.generated.h"

class UItemDefinition;
class AItemExecutionStrategy;
class UItemDistributionPolicy;
class AItemSpawnPoint;
class UItemSpawnPointRoutingConfig;

UENUM(BlueprintType)
enum class EItemSpawnRefreshMode : uint8
{
    None UMETA(DisplayName = "None"),
    TimedInfinite UMETA(DisplayName = "Timed (Infinite)"),
    TimedLimitedResets UMETA(DisplayName = "Timed (Limited Resets)")
};

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

    // --- World Spawn API ---

    /**
     * Registers a world spawn point controlled by this manager.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void RegisterSpawnPoint(AItemSpawnPoint* SpawnPoint);

    /**
     * Unregisters a world spawn point.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void UnregisterSpawnPoint(AItemSpawnPoint* SpawnPoint);

    /**
     * Re-assigns items on all registered spawn points.
     * @param bConsumeResetBudget - If true, increments the limited reset counter.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void RefreshAllSpawnPoints(bool bConsumeResetBudget = false);

    /**
     * Re-assigns the item of a single spawn point.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void RefreshSpawnPoint(AItemSpawnPoint* SpawnPoint);

    /**
     * Notifies the manager that a spawn point item was consumed.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void NotifySpawnPointItemConsumed(AItemSpawnPoint* SpawnPoint, bool bRequestImmediateRespawn);

    /**
     * Returns remaining automatic resets (limited mode only).
     * Returns -1 when the mode is not limited.
     */
    UFUNCTION(BlueprintPure, Category = "Item System|World Spawns")
    int32 GetWorldSpawnResetsRemaining() const;

    /**
     * Loads pickup/routing settings from the configured data asset and optionally propagates them.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void ApplySpawnPointPickupRoutingConfig(bool bPropagateToRegisteredSpawnPoints = true);

    /**
     * Overrides pickup/routing settings at runtime and optionally propagates them.
     */
    UFUNCTION(BlueprintCallable, Category = "Item System|World Spawns")
    void SetSpawnPointPickupRoutingSettings(
        const FItemSpawnPointPickupRoutingSettings& NewSettings,
        bool bPropagateToRegisteredSpawnPoints = true);

    UFUNCTION(BlueprintPure, Category = "Item System|World Spawns")
    FItemSpawnPointPickupRoutingSettings GetSpawnPointPickupRoutingSettings() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    
    // --- Internal Logic ---

    /**
     * Helper to load the class synchronously if not loaded.
     * In a production environment, this should ideally be async or preloaded.
     */
    UClass* ResolveExecutionClass(const TSoftClassPtr<AItemExecutionStrategy>& SoftClass);

    AItemExecutionStrategy* GetPooledActor(UClass* ExecClass);
    void AddToPool(AItemExecutionStrategy* Actor);

    // --- World Spawn Internal Logic ---

    void DiscoverSpawnPoints();
    void CleanupInvalidSpawnPoints();
    void StartWorldSpawnRefreshTimer();
    void StopWorldSpawnRefreshTimer();

    UFUNCTION()
    void HandleWorldSpawnRefreshTick();

    UItemDefinition* SelectItemForSpawnPoint(const AItemSpawnPoint* SpawnPoint) const;
    void BuildWorldSpawnCandidateList(TArray<UItemDefinition*>& OutCandidates) const;
    void ApplyPickupRoutingSettingsToSpawnPoint(AItemSpawnPoint* SpawnPoint) const;
    void ApplyActivePickupRoutingSettingsToRegisteredSpawnPoints();
    void RefreshActivePickupRoutingSettingsFromConfig();

protected:
    // Enables world spawn point management.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns")
    bool bEnableWorldSpawnManagement = true;

    // Default policy used to assign items on spawn points.
    UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly, Category = "World Spawns")
    TObjectPtr<UItemDistributionPolicy> WorldSpawnDistributionPolicy = nullptr;

    // Optional global filter applied before per-spawn constraints.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns")
    FGameplayTagQuery GlobalWorldSpawnFilter;

    // Auto-discovers placed spawn points at BeginPlay (server).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns")
    bool bAutoDiscoverSpawnPoints = true;

    // Refresh mode for spawn points.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns")
    EItemSpawnRefreshMode WorldSpawnRefreshMode = EItemSpawnRefreshMode::None;

    // Seconds between automatic refreshes.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns",
        meta = (ClampMin = "0.1", EditCondition = "WorldSpawnRefreshMode != EItemSpawnRefreshMode::None", EditConditionHides))
    float WorldSpawnRefreshInterval = 30.0f;

    // Number of timed refreshes allowed in the session (limited mode).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns",
        meta = (ClampMin = "0", EditCondition = "WorldSpawnRefreshMode == EItemSpawnRefreshMode::TimedLimitedResets", EditConditionHides))
    int32 WorldSpawnMaxResets = 0;

    // If true, consuming a spawn point item instantly requests a new one.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns")
    bool bRefreshOnConsume = true;

    // Data asset read at startup by the authoritative manager (owned by GameState).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns|Pickup Routing")
    TObjectPtr<UItemSpawnPointRoutingConfig> SpawnPointPickupRoutingConfig = nullptr;

    // Fallback settings used when no data asset is assigned.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Spawns|Pickup Routing")
    FItemSpawnPointPickupRoutingSettings SpawnPointPickupRoutingFallbackSettings;

    // Runtime list of all currently registered spawn points.
    UPROPERTY(Transient)
    TArray<TObjectPtr<AItemSpawnPoint>> RegisteredSpawnPoints;

    // Runtime counter used by limited reset mode.
    UPROPERTY(Transient, BlueprintReadOnly, Category = "World Spawns")
    int32 WorldSpawnResetsDone = 0;

    UPROPERTY(Transient)
    bool bWorldSpawnInitializationDone = false;

    UPROPERTY(Transient)
    FItemSpawnPointPickupRoutingSettings ActiveSpawnPointPickupRoutingSettings;

    FTimerHandle WorldSpawnRefreshTimerHandle;
};
