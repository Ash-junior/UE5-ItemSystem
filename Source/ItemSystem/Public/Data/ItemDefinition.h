#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Core/ItemSystemTypes.h"
#include "Data/ItemPayloadRoutingTypes.h"
#include "ItemDefinition.generated.h"

// Forward declarations
class AItemExecutionStrategy;
class UItemTargetingStrategy;
class UItemPayloadStrategy;
class USoundBase;

/**
 * Data Asset defining an Item.
 * Contains logic references, visuals, and rules.
 */
UCLASS(BlueprintType)
class ITEMSYSTEM_API UItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- Identity & Rules ---

    // Tags identifying the item (e.g., Item.Type.Offense.Projectile)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FGameplayTagContainer IdentityTags;

    // Tags on the owner that prevent usage (e.g., Status.Stunned)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules")
    FGameplayTagContainer UsageBlockingTags;

    // Name displayed in UI
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    // Visual assets (Icon, Mesh, Color)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
    FItemVisuals Visuals;

    // Maximum number of items allowed in a stack
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "1"))
    int32 MaxStack = 1;

    // Time in seconds before the item can be used again
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rules", meta = (ClampMin = "0.0"))
    float Cooldown = 0.0f;

    // Default amount granted to inventory when this item is picked up from a spawn point.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1"))
    int32 PickupGrantAmount = 1;

    // Where to attach the item model on the vehicle/character (e.g., Socket.Mount.Roof)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Visuals")
    FGameplayTag AttachSocketTag;

    // --- Logic (Soft References for Memory Management) ---

    // The actor to spawn when used (Projectile, Mine, etc.)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logic")
    TSoftClassPtr<AItemExecutionStrategy> ExecutionClass;

    // The logic used to find a target
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logic")
    TSoftClassPtr<UItemTargetingStrategy> TargetingClass;

    // The effect applied to the target
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logic")
    TSoftClassPtr<UItemPayloadStrategy> PayloadClass;

    // Rules used by direct/instant executions to route payload recipients.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Logic")
    FItemPayloadRoutingSettings PayloadRouting;

    // --- Audio & FX ---

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    USoundBase* Sound_OnEquip;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio")
    USoundBase* Sound_OnActivate;

public:
    // Overridden to allow Asset Manager to load/find items easily
    //virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
