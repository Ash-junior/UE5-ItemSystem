#pragma once

#include "CoreMinimal.h"
#include "Strategies/ExecutionStrategy.h"
#include "Execution_Projectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

/**
 * Controls how the projectile acquires its initial velocity.
 *
 * ArcThrow        — Ballistic parabola toward the player's camera aim point.
 * Drop            — Released with minimal velocity; gravity handles the trajectory.
 *                   Useful for mines or grenades dropped at the character's feet.
 * ExternalVelocity— Velocity is supplied by an external system (animation-based
 *                   throw, charge-up, etc.) via FItemContext::LaunchVelocity.
 *                   The external system must fill bHasExternalLaunchVelocity = true
 *                   and call Manager->SpawnItemExecution() directly.
 */
UENUM(BlueprintType)
enum class EItemLaunchMode : uint8
{
	ArcThrow         UMETA(DisplayName = "Arc Throw"),
	Drop             UMETA(DisplayName = "Drop"),
	ExternalVelocity UMETA(DisplayName = "External Velocity"),
};

/**
 * Concrete Execution Strategy for projectile-based items.
 * Handles movement and collision-triggered payload application.
 * Launch mode is configured per Blueprint subclass.
 */
UCLASS()
class ITEMSYSTEM_API AExecution_Projectile : public AItemExecutionStrategy
{
	GENERATED_BODY()

public:
	AExecution_Projectile();

protected:
	// Collision representation of the projectile
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	USphereComponent* CollisionComponent;

	// Handles physics and movement
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// ------------------------------------------------------------------ Launch

	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	EItemLaunchMode LaunchMode = EItemLaunchMode::ArcThrow;

	// Initial speed used by ArcThrow and as MaxSpeed for Drop/External modes.
	UPROPERTY(EditDefaultsOnly, Category = "Launch", meta = (ClampMin = "1.0"))
	float Speed = 2000.0f;

	// Gravity multiplier applied to projectile movement.
	UPROPERTY(EditDefaultsOnly, Category = "Launch")
	float GravityScale = 1.0f;

	// ------------------------------------------------------------------ Arc Throw

	// Maximum distance of the camera aim trace used to find the target point.
	UPROPERTY(EditDefaultsOnly, Category = "Launch|Arc Throw",
		meta = (ClampMin = "100.0", EditCondition = "LaunchMode == EItemLaunchMode::ArcThrow", EditConditionHides))
	float ArcTraceDistance = 5000.0f;

	// If true, prefer the higher parabolic arc when two solutions exist.
	UPROPERTY(EditDefaultsOnly, Category = "Launch|Arc Throw",
		meta = (EditCondition = "LaunchMode == EItemLaunchMode::ArcThrow", EditConditionHides))
	bool bFavorHighArc = false;

	// ------------------------------------------------------------------ Drop

	// Small forward impulse applied on drop (0 = pure vertical fall).
	UPROPERTY(EditDefaultsOnly, Category = "Launch|Drop",
		meta = (ClampMin = "0.0", EditCondition = "LaunchMode == EItemLaunchMode::Drop", EditConditionHides))
	float DropForwardImpulse = 0.0f;

	// ------------------------------------------------------------------ Debug

	// Toggle to see debug sphere and impact markers in-game.
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowDebugVisuals = true;

	// Safety: prevent multiple payload triggers from the same impact
	UPROPERTY(VisibleInstanceOnly, Category = "State")
	bool bHasExploded = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void ResetForReuse() override;

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult);

private:
	// Dispatches initial velocity setup based on LaunchMode.
	void ApplyLaunchMode();

	void ApplyArcThrow();
	void ApplyDrop();
	void ApplyExternalVelocity();

	bool CanTriggerOnActor(AActor* OtherActor) const;
	void TriggerExplosion(AActor* OtherActor, UPrimitiveComponent* OtherComp,
		const FHitResult* Hit, bool bFromOverlap);
};
