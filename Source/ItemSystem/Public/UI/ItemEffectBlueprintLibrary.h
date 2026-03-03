#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Components/ItemEffectComponent.h"
#include "GameplayTagContainer.h"
#include "ItemEffectBlueprintLibrary.generated.h"

class UItemEffectComponent;

/**
 * Blueprint-accessible helpers for building item effect and cooldown UI widgets.
 *
 * Typical widget setup:
 *   1. Bind to UItemEffectComponent::OnEffectsChanged.
 *   2. Call GetActiveEffects() to get the current snapshot.
 *   3. For each FItemActiveEffect, use GetEffectNormalizedProgress / GetEffectRemainingTime
 *      with the current server time (GetServerWorldTimeSeconds from GameState).
 *
 * Cooldown UI:
 *   Drive a progress bar directly from UInventoryComponent::GetCooldownProgress()
 *   and a text widget from GetCooldownRemainingTime(). Both are already on the component.
 */
UCLASS()
class ITEMSYSTEM_API UItemEffectBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// ------------------------------------------------------------------ Effect progress

	/**
	 * Returns how far along an effect is: 0 = just applied, 1 = expired.
	 * Always returns 1 for permanent effects (Duration <= 0).
	 *
	 * @param Effect       The active effect to evaluate.
	 * @param CurrentTime  Current world time (use GameState->GetServerWorldTimeSeconds()).
	 */
	UFUNCTION(BlueprintPure, Category = "Item System|UI", meta = (DisplayName = "Get Effect Normalized Progress"))
	static float GetEffectNormalizedProgress(const FItemActiveEffect& Effect, float CurrentTime);

	/**
	 * Returns seconds remaining before the effect expires.
	 * Returns -1 for permanent effects (Duration <= 0).
	 *
	 * @param Effect       The active effect to evaluate.
	 * @param CurrentTime  Current world time (use GameState->GetServerWorldTimeSeconds()).
	 */
	UFUNCTION(BlueprintPure, Category = "Item System|UI", meta = (DisplayName = "Get Effect Remaining Time"))
	static float GetEffectRemainingTime(const FItemActiveEffect& Effect, float CurrentTime);

	// ------------------------------------------------------------------ Effect queries

	/**
	 * Returns all active effects whose tag matches (or is a child of) the given tag.
	 * Example: passing "Item.Effect.ModifySpeed" returns both Boost and Slow entries.
	 *
	 * @param Component  The effect component to query (must be valid).
	 * @param Tag        The parent tag to filter by.
	 */
	UFUNCTION(BlueprintPure, Category = "Item System|UI", meta = (DisplayName = "Get Effects By Tag"))
	static TArray<FItemActiveEffect> GetEffectsByTag(UItemEffectComponent* Component, FGameplayTag Tag);

	/**
	 * Returns true if at least one effect matching (or child of) Tag is currently active.
	 *
	 * @param Component  The effect component to query.
	 * @param Tag        The tag to check against.
	 */
	UFUNCTION(BlueprintPure, Category = "Item System|UI", meta = (DisplayName = "Has Active Effect"))
	static bool HasActiveEffect(UItemEffectComponent* Component, FGameplayTag Tag);
};
