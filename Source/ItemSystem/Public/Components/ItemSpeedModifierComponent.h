#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ItemSpeedModifierComponent.generated.h"

class UCharacterMovementComponent;

USTRUCT()
struct FSpeedModifierEntry
{
	GENERATED_BODY()

	float Multiplier = 1.0f;
	FTimerHandle TimerHandle;
};

/**
 * Tracks and applies tagged movement speed modifiers on a Character.
 * Modifiers with the same tag override/refresh instead of stacking.
 */
UCLASS(ClassGroup = (ItemSystem), meta = (BlueprintSpawnableComponent))
class ITEMSYSTEM_API UItemSpeedModifierComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Item System")
	void ApplySpeedModifier(UCharacterMovementComponent* MoveComp, FGameplayTag Tag, float Multiplier, float Duration);

private:
	bool bHasBaseSpeed = false;
	float BaseSpeed = 0.0f;
	TMap<FGameplayTag, FSpeedModifierEntry> ActiveModifiers;

private:
	UCharacterMovementComponent* GetMoveComp() const;
	void ClearModifier(FGameplayTag Tag);
	void RecomputeSpeed(UCharacterMovementComponent* MoveComp);
};
