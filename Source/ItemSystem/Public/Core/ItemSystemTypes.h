#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/HitResult.h"
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

    // Optional pre-computed launch velocity provided by an aim/throw system.
    // Projectile executions consume it when bHasExternalLaunchVelocity is true.
    UPROPERTY(BlueprintReadWrite, Category = "Context")
    FVector LaunchVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Context")
    bool bHasExternalLaunchVelocity = false;
};

/**
 * Tunable values used by both aim preview and projectile launch calculation.
 * Keeping these values in one struct prevents the displayed arc from drifting
 * away from the velocity used by the spawned projectile.
 */
USTRUCT(BlueprintType)
struct FItemProjectileArcParams
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    FVector StartLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    FVector ViewLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    FRotator ViewRotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    FVector AimPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc", meta = (ClampMin = "1.0"))
    float Speed = 2000.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    float GravityScale = 1.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc", meta = (ClampMin = "100.0"))
    float TraceDistance = 5000.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc", meta = (ClampMin = "0.0"))
    float ProjectileRadius = 15.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc", meta = (ClampMin = "0.1"))
    float MaxSimTime = 3.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc", meta = (ClampMin = "1.0"))
    float SimFrequency = 15.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    bool bFavorHighArc = false;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    bool bTraceWithCollision = true;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    UPROPERTY(BlueprintReadWrite, Category = "Projectile Arc")
    TArray<AActor*> ActorsToIgnore;
};

/**
 * Compact launch payload captured while aiming. Clients send this to the server
 * so item activation can reuse the last displayed ballistic solution.
 */
USTRUCT(BlueprintType)
struct FItemAimData
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, Category = "Aim")
    bool bIsValid = false;

    UPROPERTY(BlueprintReadWrite, Category = "Aim")
    FVector StartLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Aim")
    FVector AimPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Aim")
    FVector LaunchVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Aim")
    FVector ViewLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Aim")
    FRotator ViewRotation = FRotator::ZeroRotator;
};

/**
 * Result used by UI/FX code to draw the predicted arc and impact marker.
 */
USTRUCT(BlueprintType)
struct FItemProjectileArcResult
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    bool bHasSolution = false;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    bool bHit = false;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    FVector AimPoint = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    FVector LaunchVelocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    FVector TracedPosition = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    FVector TracedNormal = FVector::UpVector;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    TArray<FVector> PathPoints;

    UPROPERTY(BlueprintReadOnly, Category = "Projectile Arc")
    FHitResult HitResult;
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
