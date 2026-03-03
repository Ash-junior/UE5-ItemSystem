#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/ItemSystemTypes.h"
#include "ExecutionStrategy.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USoundBase;

class UItemTargetingStrategy;
class UItemPayloadStrategy;

/**
 * The physical representation of the item in the world (Projectile, Trap, etc.).
 * Responsible for movement, collision, and executing the Payload on hit.
 */
UCLASS(Abstract, Blueprintable)
class ITEMSYSTEM_API AItemExecutionStrategy : public AActor
{
	GENERATED_BODY()

public:
	AItemExecutionStrategy();

	// Called by the manager when this actor is reused from the pool.
	UFUNCTION(BlueprintCallable, Category = "Item System")
	virtual void ResetForReuse();

protected:
	// The context passed from the inventory when spawned.
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Item System", Meta = (ExposeOnSpawn = "true"))
	FItemContext ItemContext;

	// Default trail VFX
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UNiagaraSystem* TrailVFX;

	// Sound played when this actor spawns
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	USoundBase* SpawnSound;

	// VFX played when the execution triggers (impact/overlap)
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	UNiagaraSystem* ImpactVFX;

	// Sound played when the execution triggers (impact/overlap)
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	USoundBase* ImpactSound;
	
	// Active trail VFX component — tracked so it can be deactivated on pool reuse.
	UPROPERTY()
	UNiagaraComponent* ActiveTrailVFX = nullptr;

	// Instances created at runtime based on the ItemDefinition
	UPROPERTY()
	UItemTargetingStrategy* TargetingInstance;

	UPROPERTY()
	UItemPayloadStrategy* PayloadInstance;
	
public:
	// Helper to allow the Manager to set context before BeginPlay
	void SetItemContext(const FItemContext& InContext) { ItemContext = InContext; }	

	// Helper to get the context
	UFUNCTION(BlueprintPure, Category = "Item System")
	const FItemContext& GetItemContext() const { return ItemContext; }
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	// Called when the execution is finished (e.g., hit target) to cleanup.
	UFUNCTION(BlueprintCallable, Category = "Item System")
	void FinishExecution();

	// Play impact VFX/SFX (server triggers multicast)
	void PlayImpactFX(const FVector& Location);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayImpactFX(const FVector& Location);

	void SpawnImpactFX(const FVector& Location);

	// Helper: should the item affect the other actor (team/immunity/context)
	bool ShouldAffectActor(AActor* OtherActor) const;

	// Plays spawn sound and (re)attaches trail VFX. Called from BeginPlay and
	// from the base ResetForReuse so pooled actors replay effects on reuse.
	void PlaySpawnEffects();
};
