#pragma once

#include "CoreMinimal.h"
#include "Strategies/ItemTargetingStrategy.h"
#include "Targeting_FindNearest.generated.h"

/**
 * Concrete Targeting Strategy: Finds the closest actor with a specific tag.
 */
UCLASS(meta = (DisplayName = "Targeting: Find Nearest"))
class ITEMSYSTEM_API UTargeting_FindNearest : public UItemTargetingStrategy
{
	GENERATED_BODY()

public:
	// Radius to search for targets around the Origin
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config")
	float SearchRadius = 2000.0f;

	// Only target actors with this specific tag (Actor Tag, not GameplayTag for simplicity here)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting Config")
	FName RequiredActorTag = NAME_None;

public:
	virtual AActor* FindTarget_Implementation(const FItemContext& Context, FVector Origin) override;
};



