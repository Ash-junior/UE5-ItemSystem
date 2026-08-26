#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ItemSystemTypes.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.generated.h"

class UItemDefinition;
class UStaticMeshComponent;
class USoundBase;

// Delegate used to notify UI or other systems when ammo changes
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryChanged, UItemDefinition*, Item, int32, NewAmmo);

/**
 * Component responsible for holding an item, managing ammo,
 * and handling the visual representation (Held Mesh) on the owner.
 */
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ITEMSYSTEM_API UInventoryComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UInventoryComponent();

protected:
    // --- State Variables (Replicated) ---

    // The current item definition loaded.
    // Uses RepNotify to update visuals on clients immediately when changed.
    UPROPERTY(ReplicatedUsing = OnRep_CurrentItem, BlueprintReadOnly, Category = "Inventory")
    UItemDefinition* CurrentItem;

    // Current stack count.
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
    int32 CurrentAmmo;

    // Last time the current item was activated (server time).
    // Replicated to owner so the client can drive the cooldown progress bar.
    UPROPERTY(Replicated)
    float LastActivationTime = -FLT_MAX;

    // --- Components ---

    // The mesh component representing the item visually (e.g. a missile on the roof).
    // This is created dynamically or referenced from the owner.
    UPROPERTY()
    UStaticMeshComponent* HeldMeshComponent;

public:
    // Event fired when Item or Ammo changes (useful for UI)
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInventoryChanged OnInventoryChanged;

    // --- Public API ---

    /**
     * [Server Only] Grants an item to this inventory.
     * @param NewItem - The item data asset to give.
     * @param Amount - How much ammo to add.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Inventory")
    void Server_GrantItem(UItemDefinition* NewItem, int32 Amount = 1);

    /**
     * [Server Only] Attempts to use the current item.
     * Checks cooldowns, blocking tags, and ammo before executing.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Inventory")
    void Server_TryActivateItem();

    /**
     * [Server Only] Attempts to use the current item with a precomputed aim arc.
     * Use this after an aim-preview component has produced FItemAimData.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Inventory")
    void Server_TryActivateItemWithAim(const FItemAimData& AimData);

    /**
     * Helper to get the current item data.
     */
    UFUNCTION(BlueprintPure, Category = "Inventory")
    UItemDefinition* GetCurrentItem() const { return CurrentItem; }

    UFUNCTION(BlueprintPure, Category = "Inventory")
    int32 GetCurrentAmmo() const { return CurrentAmmo; }

    // ------------------------------------------------------------------ Cooldown UI

    // Returns 0 (just activated / blocked) to 1 (ready). Always 1 when there is no cooldown.
    UFUNCTION(BlueprintPure, Category = "Inventory|Cooldown")
    float GetCooldownProgress() const;

    // Returns seconds remaining until the item is ready again. 0 when ready.
    UFUNCTION(BlueprintPure, Category = "Inventory|Cooldown")
    float GetCooldownRemainingTime() const;

    // True while the item is on cooldown and cannot be activated.
    UFUNCTION(BlueprintPure, Category = "Inventory|Cooldown")
    bool IsOnCooldown() const;

    /**
     * [Server Only] Clears current item and ammo.
     */
    UFUNCTION(BlueprintCallable, Server, Reliable, Category = "Inventory")
    void Server_ClearInventory();

protected:
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_PlayActivateSound(USoundBase* Sound, FVector Location);

protected:
    // --- Internal Logic ---

    virtual void BeginPlay() override;
    
    // Required for network replication
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Called on clients when CurrentItem changes
    UFUNCTION()
    void OnRep_CurrentItem();

    /**
     * Updates the HeldMeshComponent based on the current item.
     * @param bVisible - Should the mesh be visible?
     */
    void UpdateVisuals(bool bVisible);

    /**
     * Checks if the owner is allowed to use the item (e.g. not stunned).
     */
    bool CanUseItem() const;

    void TryActivateItemInternal(const FItemAimData* AimData);

    bool CanUseAimData(const FItemAimData& AimData, const FItemContext& Context) const;

    /**
     * Creates the context struct to pass to the Item System Manager.
     */
    FItemContext MakeItemContext() const;
};
