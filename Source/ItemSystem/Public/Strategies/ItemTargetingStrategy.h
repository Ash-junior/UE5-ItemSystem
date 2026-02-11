#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/ItemSystemTypes.h"
#include "ItemTargetingStrategy.generated.h"

/**
 * Base class for targeting logic.
 * Determines "who" or "what" the item should affect.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class ITEMSYSTEM_API UItemTargetingStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Finds the best target based on the item context and origin.
	 * @param Context - The full context of the item usage.
	 * @param Origin - The location from where the search starts.
	 * @return The best actor found, or nullptr if none.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Item Strategy")
	AActor* FindTarget(const FItemContext& Context, FVector Origin);

	virtual AActor* FindTarget_Implementation(const FItemContext& Context, FVector Origin)
	{
		return nullptr;
	}
};