#pragma once

#include "CoreMinimal.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Payload_Explosion.generated.h"

/**
 * Concrete Payload Strategy: Applies radial damage around an origin.
 */
UCLASS(meta = (DisplayName = "Payload: Explosion"))
class ITEMSYSTEM_API UPayload_Explosion : public UItemPayloadStrategy
{
	GENERATED_BODY()

public:
	// Base damage applied to actors in the radius
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config")
	float DamageAmount = 20.0f;

	// Radius of the explosion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config", meta = (ClampMin = "0.0"))
	float DamageRadius = 300.0f;

	// The type of damage (useful for resistance/immunity logic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config")
	TSubclassOf<UDamageType> DamageTypeClass;

	// If true, full damage is applied regardless of distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config")
	bool bDoFullDamage = false;

	// If true, the instigator is ignored by the explosion
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config")
	bool bIgnoreInstigator = true;

public:
	virtual void ApplyEffect_Implementation(AActor* Target, const FItemContext& Context) override;
};
