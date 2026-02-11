#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "TargetableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UTargetableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by actors that can be targeted by items.
 */
class ITEMSYSTEM_API ITargetableInterface
{
	GENERATED_BODY()

public:
	/**
	 * Returns the ideal center point for aiming (e.g., center of mass).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	FVector GetLockOnLocation() const;

	/**
	 * Checks if this actor is immune to an item based on incoming tags.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	bool IsImmuneTo(const FGameplayTagContainer& IncomingTags) const;
};