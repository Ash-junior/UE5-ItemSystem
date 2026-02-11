#include "Distribution/DistributionPolicy_Random.h"

#include "Data/ItemDefinition.h"

UItemDefinition* UDistributionPolicy_Random::SelectItem_Implementation(AActor* Requester, const TArray<UItemDefinition*>& Items) const
{
	TArray<UItemDefinition*> ValidItems;
	ValidItems.Reserve(Items.Num());

	for (UItemDefinition* Item : Items)
	{
		if (Item)
		{
			ValidItems.Add(Item);
		}
	}

	if (ValidItems.Num() == 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, ValidItems.Num() - 1);
	return ValidItems[Index];
}
