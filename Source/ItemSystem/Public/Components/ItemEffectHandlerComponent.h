#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ItemSystemTypes.h"
#include "ItemEffectHandlerComponent.generated.h"

class UItemEffectComponent;

/**
 * Component to attach to a pawn. Routes FItemEffectSpec by tag and applies
 * the corresponding effect. Each apply function is BlueprintNativeEvent so
 * Blueprint subclasses can override the behavior per effect type.
 *
 * Optionally notifies a sibling UItemEffectComponent (for UI tracking).
 */
UCLASS(Blueprintable, ClassGroup = (ItemSystem), meta = (BlueprintSpawnableComponent))
class ITEMSYSTEM_API UItemEffectHandlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemEffectHandlerComponent();

	/**
	 * Main entry point. Called by the pawn's IItemInterface::ApplyItemEffect.
	 * Returns true if the effect tag was recognized and handled.
	 */
	UFUNCTION(BlueprintCallable, Category = "Item System")
	bool HandleEffect(const FItemEffectSpec& Spec, const FItemContext& Context);

	// --- Per-effect apply functions (override in Blueprint or C++) ----------

	UFUNCTION(BlueprintNativeEvent, Category = "Item System")
	void ApplySpeedEffect(float Multiplier, float Duration, FGameplayTag EffectTag);

	// --- Tag routing config -------------------------------------------------

	// Matches any tag under this parent (e.g. Item.Effect.ModifySpeed.Boost also matches)
	UPROPERTY(EditAnywhere, Category = "Item System|Tags")
	FGameplayTag SpeedEffectParentTag;

protected:
	virtual void BeginPlay() override;

	virtual void ApplySpeedEffect_Implementation(float Multiplier, float Duration, FGameplayTag EffectTag);

private:
	// One active instance of a speed effect with its own timer
	struct FActiveSpeedEffect
	{
		uint32 EffectID = 0;
		FGameplayTag Tag;
		float Multiplier = 1.0f;
		FTimerHandle TimerHandle;
	};

	// Recalculates and applies BaseSpeed * product(all active multipliers)
	void RecalculateAndApplySpeed();

	// Called when the timer for a specific effect instance fires
	void OnSpeedEffectExpiredByID(uint32 EffectID);

	UItemEffectComponent* FindEffectComponent() const;

	// Speed effect runtime state
	float BaseSpeed = 0.0f;
	bool bHasBaseSpeed = false;
	uint32 NextSpeedEffectID = 0;
	TArray<FActiveSpeedEffect> ActiveSpeedEffects;
};