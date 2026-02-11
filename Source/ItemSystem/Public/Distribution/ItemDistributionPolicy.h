#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemDistributionPolicy.generated.h"

class UItemDefinition;
class AActor;

/**
 * Base class for item distribution policies.
 * Determines which item is selected from a list for a given requester.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class ITEMSYSTEM_API UItemDistributionPolicy : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	UItemDefinition* SelectItem(AActor* Requester, const TArray<UItemDefinition*>& Items) const;

	virtual UItemDefinition* SelectItem_Implementation(AActor* Requester, const TArray<UItemDefinition*>& Items) const
	{
		return nullptr;
	}
};
