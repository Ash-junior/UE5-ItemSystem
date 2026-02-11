#include "Strategies/Implementation/Targeting_Raycast.h"
#include "Engine/World.h"

AActor* UTargeting_Raycast::FindTarget_Implementation(const FItemContext& Context, FVector Origin)
{
	if (!Context.Instigator) return nullptr;

	UWorld* World = Context.Instigator->GetWorld();
	if (!World) return nullptr;

	// Determine direction (Actor Forward Vector)
	FVector Forward = Context.Instigator->GetActorForwardVector();
    
	// If we have a controller with a camera, maybe use camera forward?
	// For simplicity, we stick to Actor Forward for now.

	FVector Start = Origin;
	FVector End = Start + (Forward * TraceDistance);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Context.Instigator); // Don't hit yourself

	// Perform Raycast
	bool bHit = World->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility, // Trace against visible objects
		Params
	);

	if (bHit)
	{
		return HitResult.GetActor();
	}

	return nullptr;
}

