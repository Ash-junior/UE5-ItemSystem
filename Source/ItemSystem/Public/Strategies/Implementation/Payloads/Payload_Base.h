#pragma once

#include "CoreMinimal.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Core/ItemInterface.h"
#include "Payload_Base.generated.h"


/**
 * Basic Payload Strategy
 * Dispatches via IItemInterface::ApplyItemEffect — the pawn's
 * UItemEffectHandlerComponent handles the actual application.
 */
UCLASS()
class ITEMSYSTEM_API UPayload_Base : public UItemPayloadStrategy
{
	GENERATED_BODY()

public:
	UPayload_Base();
	
	// Payload magnitude
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payload", meta = (ClampMin = "0.0"))
	float Magnitude = 1.0f;
	
	// Duration in seconds before restoring the value
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payload", meta = (ClampMin = "0.0"))
	float Duration = 3.0f;
	
	// Tag forwarded to the handler for routing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payload")
	FGameplayTag EffectTag;
	
public:
	virtual void ApplyEffect_Implementation(AActor* Target, const FItemContext& Context) override;
};
