#pragma once

#include "CoreMinimal.h"
#include "Distribution/ItemDistributionPolicy.h"
#include "DistributionPolicy_Random.generated.h"

/**
 * Selects a random item from the provided list.
 */
UCLASS(meta = (DisplayName = "Policy: Random"))
class ITEMSYSTEM_API UDistributionPolicy_Random : public UItemDistributionPolicy
{
	GENERATED_BODY()

public:
	virtual UItemDefinition* SelectItem_Implementation(AActor* Requester, const TArray<UItemDefinition*>& Items) const override;
};
