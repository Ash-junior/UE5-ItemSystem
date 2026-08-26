#pragma once

#include "CoreMinimal.h"
#include "Strategies/ExecutionStrategy.h"
#include "Engine/HitResult.h"
#include "Execution_Trap.generated.h"

class USphereComponent;

/**
 * Stationary trap (mine) that triggers on overlap and applies its payload.
 */
UCLASS()
class ITEMSYSTEM_API AExecution_Trap : public AItemExecutionStrategy
{
	GENERATED_BODY()

public:
	AExecution_Trap();

protected:
	// Collision representation of the trap trigger
	UPROPERTY(VisibleDefaultsOnly, Category = "Components")
	USphereComponent* TriggerComponent;

	// Radius that will trigger the trap
	UPROPERTY(EditDefaultsOnly, Category = "Trap Config", meta = (ClampMin = "0.0"))
	float TriggerRadius = 150.0f;

	// Optional auto-destroy if not triggered
	UPROPERTY(EditDefaultsOnly, Category = "Trap Config", meta = (ClampMin = "0.0"))
	float LifeSpanSeconds = 30.0f;

	// Toggle this to see debug spheres in-game
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	bool bShowDebugVisuals = true;

	// Safety: prevent multiple triggers from overlapping events
	UPROPERTY(VisibleInstanceOnly, Category = "Trap State")
	bool bHasTriggered = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void ResetForReuse() override;

	/**
	 * Proximity trigger: fired when overlapping a valid target.
	 */
	UFUNCTION()
	void OnTrapOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool CanTriggerOnActor(AActor* OtherActor) const;
	void TriggerTrap(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FHitResult* Hit);
};
