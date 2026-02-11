#include "Strategies/Implementation/Targetings/Targeting_FindNearest.h"

#include "Core/ItemInterface.h"
#include "Core/TargetableInterface.h"
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

	const bool bIgnoreTeammates = Context.ItemDefinition && Context.ItemDefinition->IdentityTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Rule.Ignore.Teammates"), false));
	int32 InstigatorTeam = INDEX_NONE;
	if (bIgnoreTeammates && Context.Instigator && Context.Instigator->Implements<UItemInterface>())
	{
		InstigatorTeam = IItemInterface::Execute_GetTeamID(Context.Instigator);
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

		if (bIgnoreTeammates && InstigatorTeam != INDEX_NONE && Actor->Implements<UItemInterface>())
		{
			if (IItemInterface::Execute_GetTeamID(Actor) == InstigatorTeam)
			{
				continue;
			}
		}

		// Immunity check (if target rejects incoming tags)
		if (Actor->Implements<UTargetableInterface>())
		{
			if (ITargetableInterface::Execute_IsImmuneTo(Actor, Context.ContextTags))
			{
				continue;
			}
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
