#pragma once

#include "CoreMinimal.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "GameFramework/DamageType.h"
#include "Payload_Damage.generated.h"

/**
 * Concrete Payload Strategy: Applies direct damage to the target.
 */
UCLASS(meta = (DisplayName = "Payload: Simple Damage"))
class ITEMSYSTEM_API UPayload_Damage : public UItemPayloadStrategy
{
	GENERATED_BODY()

public:
	// Amount of damage to apply
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config")
	float DamageAmount = 10.0f;

	// The type of damage (useful for resistance/immunity logic)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage Config")
	TSubclassOf<UDamageType> DamageTypeClass;

public:
	virtual void ApplyEffect_Implementation(AActor* Target, const FItemContext& Context) override;
};
