#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Payload_ModifySpeed.generated.h"

/**
 * Concrete Payload Strategy: Temporarily modifies the target's MaxWalkSpeed.
 * Use SpeedMultiplier < 1.0 for slow, > 1.0 for buff.
 */
UCLASS(meta = (DisplayName = "Payload: Modify Speed"))
class ITEMSYSTEM_API UPayload_ModifySpeed : public UItemPayloadStrategy
{
	GENERATED_BODY()

public:
	UPayload_ModifySpeed();

	// Multiplier applied to the target's MaxWalkSpeed (e.g., 0.5 = 50% slow, 1.5 = 50% buff)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Config", meta = (ClampMin = "0.0"))
	float SpeedMultiplier = 0.5f;

	// Duration in seconds before restoring the original speed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Config", meta = (ClampMin = "0.0"))
	float Duration = 3.0f;

	// Tag used to group/stack modifiers. Same tag overrides/refreshes instead of stacking.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed Config")
	FGameplayTag EffectTag;

public:
	virtual void ApplyEffect_Implementation(AActor* Target, const FItemContext& Context) override;
};
