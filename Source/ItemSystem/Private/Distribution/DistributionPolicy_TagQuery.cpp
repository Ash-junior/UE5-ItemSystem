#include "Distribution/DistributionPolicy_TagQuery.h"

#include "Data/ItemDefinition.h"

UItemDefinition* UDistributionPolicy_TagQuery::SelectItem_Implementation(AActor* Requester, const TArray<UItemDefinition*>& Items) const
{
	if (Query.IsEmpty())
	{
		return nullptr;
	}

	TArray<UItemDefinition*> MatchingItems;
	MatchingItems.Reserve(Items.Num());

	for (UItemDefinition* Item : Items)
	{
		if (Item && Query.Matches(Item->IdentityTags))
		{
			MatchingItems.Add(Item);
		}
	}

	if (MatchingItems.Num() == 0)
	{
		return nullptr;
	}

	const int32 Index = FMath::RandRange(0, MatchingItems.Num() - 1);
	return MatchingItems[Index];
}
