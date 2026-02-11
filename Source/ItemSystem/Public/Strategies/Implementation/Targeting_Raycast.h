#pragma once

#include "CoreMinimal.h"
#include "Strategies/ItemTargetingStrategy.h"
#include "Targeting_Raycast.generated.h"

/**
 * Finds a target by performing a simple line trace (raycast) forward.
 */
UCLASS()
class ITEMSYSTEM_API UTargeting_Raycast : public UItemTargetingStrategy
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	float TraceDistance = 2000.0f;

	virtual AActor* FindTarget_Implementation(const FItemContext& Context, FVector Origin) override;
};