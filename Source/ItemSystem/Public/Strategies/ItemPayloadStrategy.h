#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Core/ItemSystemTypes.h"
#include "ItemPayloadStrategy.generated.h"

/**
 * Base class for the actual effect of the item (The "Payload").
 * Defines what happens when the item hits or activates.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class ITEMSYSTEM_API UItemPayloadStrategy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Applies the effect to a specific target.
	 * @param Target - The actor receiving the effect.
	 * @param Context - The context of the item used.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Item Strategy")
	void ApplyEffect(AActor* Target, const FItemContext& Context);

	virtual void ApplyEffect_Implementation(AActor* Target, const FItemContext& Context)
	{
		// Override in blueprints or subclasses
	}
};