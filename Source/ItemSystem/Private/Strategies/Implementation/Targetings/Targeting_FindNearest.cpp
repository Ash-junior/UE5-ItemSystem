#include "Strategies/Implementation/Targetings/Targeting_FindNearest.h"

#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"

AActor* UTargeting_FindNearest::FindTarget_Implementation(const FItemContext& Context, FVector Origin)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	// Determine what to search for
	TArray<AActor*> Candidates;
	
	if (!RequiredActorTag.IsNone())
	{
		// Find all actors with the tag
		UGameplayStatics::GetAllActorsWithTag(World, RequiredActorTag, Candidates);
	}
	else
	{
		// Fallback: Find all Pawns (generic enemies/players)
		// Note: In a real game, you would filter by Interface or Team ID here.
		TArray<AActor*> AllPawns;
		UGameplayStatics::GetAllActorsOfClass(World, APawn::StaticClass(), AllPawns);
		Candidates = AllPawns;
	}

	AActor* BestTarget = nullptr;
	float ClosestDistanceSq = FMath::Square(SearchRadius); // Use squared distance for optimization

	for (AActor* Actor : Candidates)
	{
		// Skip self (Instigator) and invalid actors
		if (!Actor || Actor == Context.Instigator)
		{
			continue;
		}

		float DistSq = FVector::DistSquared(Origin, Actor->GetActorLocation());
		if (DistSq < ClosestDistanceSq)
		{
			ClosestDistanceSq = DistSq;
			BestTarget = Actor;
		}
	}

	return BestTarget;
}