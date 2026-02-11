#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ItemSystemTypes.generated.h"

// Forward declaration to avoid circular dependencies
class UItemDefinition;
class AActor;
class AController;

/**
 * Visual assets associated with an item.
 * Used for UI and physical representation before activation.
 */
USTRUCT(BlueprintType)
struct FItemVisuals
{
    GENERATED_BODY()

public:
    // Icon used in the UI
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    UTexture2D* Icon = nullptr;

    // Mesh attached to the vehicle/character when the item is equipped but not used
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    UStaticMesh* HeldMesh = nullptr;

    // Theme color (used for trails, UI borders, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visuals")
    FLinearColor ColorTheme = FLinearColor::White;
};

/**
 * Universal payload passed through the entire item pipeline.
 * Contains everything an execution logic needs to know.
 */
USTRUCT(BlueprintType)
struct FItemContext
{
    GENERATED_BODY()

public:
    // The pawn that initiated the item usage
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    AActor* Instigator = nullptr;

    // The controller responsible for the action (useful for scoring)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    AController* InstigatorController = nullptr;

    // A specific target actor (if locked on)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    AActor* TargetActor = nullptr;

    // Exact location and rotation where the item was spawned/fired
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FTransform OriginTransform;

    // Precise impact location (if available, e.g., projectile hit/overlap)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    bool bHasImpactPoint = false;

    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FVector ImpactPoint = FVector::ZeroVector;

    // Reference to the item data asset (identifying what this item is)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    UItemDefinition* ItemDefinition = nullptr;

    // Contextual tags (e.g., Environment.Water, Modifier.DoubleDamage)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FGameplayTagContainer ContextTags;

    // Seed for deterministic randomness (VFX, trajectory spread)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    int32 RandomSeed = 0;

    // Unique ID for this specific usage instance (useful for debug logs/replays)
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FGuid InvocationGUID;
};

/**
 * Generic effect payload for targets to interpret (e.g., speed, shield, dot).
 */
USTRUCT(BlueprintType)
struct FItemEffectSpec
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    FGameplayTag EffectTag;

    // Interpretation depends on EffectTag (e.g., speed multiplier, damage per tick)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    float Magnitude = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
    float Duration = 0.0f;
};

/**
 * Wrapper struct for Actor Pooling.
 * Required because Unreal Header Tool does not support nested containers 
 * (e.g. TMap<UClass*, TArray<AActor*>>) in UPROPERTY.
 */
USTRUCT(BlueprintType)
struct FItemActorPool
{
    GENERATED_BODY()

public:
    // List of actors currently inactive and ready to be reused
    UPROPERTY()
    TArray<AActor*> InactiveActors;
};
