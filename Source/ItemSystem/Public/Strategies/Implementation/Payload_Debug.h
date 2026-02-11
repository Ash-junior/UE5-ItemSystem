#pragma once

#include "CoreMinimal.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Payload_Debug.generated.h"

/**
 * Simple payload that prints a message to the screen.
 * Used for debugging the pipeline.
 */
UCLASS()
class ITEMSYSTEM_API UPayload_Debug : public UItemPayloadStrategy
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	FString DebugMessage = "BOOM! Payload Activated.";

	UPROPERTY(EditDefaultsOnly, Category = "Debug")
	FColor TextColor = FColor::Green;

	virtual void ApplyEffect_Implementation(AActor* Target, const FItemContext& Context) override;
};