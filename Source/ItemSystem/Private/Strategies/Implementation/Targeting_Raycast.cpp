#include "Strategies/Implementation/Targeting_Raycast.h"
#include "Core/ItemInterface.h"
#include "Core/TargetableInterface.h"
#include "Data/ItemDefinition.h"
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
		AActor* HitActor = HitResult.GetActor();
		if (!HitActor)
		{
			return nullptr;
		}

		const bool bIgnoreTeammates = Context.ItemDefinition && Context.ItemDefinition->IdentityTags.HasTag(FGameplayTag::RequestGameplayTag(TEXT("Rule.Ignore.Teammates"), false));
		if (bIgnoreTeammates && Context.Instigator && Context.Instigator->Implements<UItemInterface>() && HitActor->Implements<UItemInterface>())
		{
			const int32 InstigatorTeam = IItemInterface::Execute_GetTeamID(Context.Instigator);
			const int32 HitTeam = IItemInterface::Execute_GetTeamID(HitActor);
			if (InstigatorTeam != INDEX_NONE && InstigatorTeam == HitTeam)
			{
				return nullptr;
			}
		}

		if (HitActor->Implements<UTargetableInterface>())
		{
			if (ITargetableInterface::Execute_IsImmuneTo(HitActor, Context.ContextTags))
			{
				return nullptr;
			}
		}

		return HitActor;
	}

	return nullptr;
}

