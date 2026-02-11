#pragma once

#include "CoreMinimal.h"
#include "Distribution/ItemDistributionPolicy.h"
#include "GameplayTagContainer.h"
#include "DistributionPolicy_TagQuery.generated.h"

/**
 * Selects a random item matching a GameplayTagQuery.
 */
UCLASS(meta = (DisplayName = "Policy: Tag Query"))
class ITEMSYSTEM_API UDistributionPolicy_TagQuery : public UItemDistributionPolicy
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Policy")
	FGameplayTagQuery Query;

	virtual UItemDefinition* SelectItem_Implementation(AActor* Requester, const TArray<UItemDefinition*>& Items) const override;
};
