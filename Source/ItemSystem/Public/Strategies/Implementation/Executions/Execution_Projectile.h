#pragma once

#include "CoreMinimal.h"
#include "Strategies/ExecutionStrategy.h"
#include "Execution_Projectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

/**
 * Concrete Execution Strategy for projectile-based items.
 * Handles movement, homing logic via Targeting Strategy, and collision triggering Payload.
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

	// Handles physics, movement, and homing capabilities
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// Base speed of the projectile
	UPROPERTY(EditDefaultsOnly, Category = "Projectile Config")
	float Speed = 2000.f;

	// If true, will attempt to find a target via TargetingStrategy and follow it
	UPROPERTY(EditDefaultsOnly, Category = "Projectile Config")
	bool bIsHoming = false;

	// Acceleration magnitude towards the target (if Homing is enabled)
	UPROPERTY(EditDefaultsOnly, Category = "Projectile Config", meta = (EditCondition = "bIsHoming"))
	float HomingAcceleration = 1000.f;

	// Gravity scale applied to the projectile movement
	UPROPERTY(EditDefaultsOnly, Category = "Projectile Config")
	float GravityScale = 1.0f;
	
	// Toggle this to see debug lines and spheres in-game
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowDebugVisuals = true;

	// Safety: prevent multiple explosions from overlapping events
	UPROPERTY(VisibleInstanceOnly, Category = "Projectile State")
	bool bHasExploded = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override; // Added for Debug Drawing

	/**
	 * Bound to the SphereComponent's OnComponentHit.
	 * Triggered when the projectile impacts a valid blocking object.
	 */
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/**
	 * Proximity fuse: triggered when overlapping a valid target.
	 */
	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool CanTriggerOnActor(AActor* OtherActor) const;
	void TriggerExplosion(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FHitResult* Hit, bool bFromOverlap);
};
