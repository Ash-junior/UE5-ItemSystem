#include "Strategies/Implementation/Payloads/Payload_Explosion.h"

#include "Kismet/GameplayStatics.h"

void UPayload_Explosion::ApplyEffect_Implementation(AActor* Target, const FItemContext& Context)
{
	const UObject* WorldContext = nullptr;
	if (Target)
	{
		WorldContext = Target;
	}
	else if (Context.Instigator)
	{
		WorldContext = Context.Instigator;
	}

	if (!WorldContext)
	{
		return;
	}

	FVector Origin = Context.OriginTransform.GetLocation();
	if (Context.bHasImpactPoint)
	{
		Origin = Context.ImpactPoint;
	}
	else if (Target)
	{
		Origin = Target->GetActorLocation();
	}
	else if (Context.Instigator)
	{
		Origin = Context.Instigator->GetActorLocation();
	}

	TArray<AActor*> IgnoreActors;
	if (bIgnoreInstigator && Context.Instigator)
	{
		IgnoreActors.Add(Context.Instigator);
	}

	UGameplayStatics::ApplyRadialDamage(
		WorldContext,
		DamageAmount,
		Origin,
		DamageRadius,
		DamageTypeClass,
		IgnoreActors,
		Context.Instigator,
		Context.InstigatorController,
		bDoFullDamage
	);
}
