#include "Strategies/Implementation/Execution_Instant.h"
#include "Strategies/ItemTargetingStrategy.h"
#include "Strategies/ItemPayloadStrategy.h"

void AExecution_Instant::BeginPlay()
{
	Super::BeginPlay(); // Instantiates strategies (Base class logic)

	// 1. Find Target
	AActor* FoundTarget = nullptr;
	if (TargetingInstance)
	{
		// Use current location as origin
		FoundTarget = TargetingInstance->FindTarget(ItemContext, GetActorLocation());
	}

	// 2. Apply Payload
	if (PayloadInstance)
	{
		if (FoundTarget && !ShouldAffectActor(FoundTarget))
		{
			FinishExecution();
			return;
		}

		// Apply to found target (can be null if strategy allows it, e.g. AOE)
		PayloadInstance->ApplyEffect(FoundTarget, ItemContext);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Execution_Instant: No Payload Strategy found!"));
	}

	// 3. Cleanup immediately
	FinishExecution();
}

void AExecution_Instant::ResetForReuse()
{
	// No state to reset for now.
}

