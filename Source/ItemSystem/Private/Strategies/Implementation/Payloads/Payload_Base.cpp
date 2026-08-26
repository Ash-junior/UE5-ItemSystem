#include "Strategies/Implementation/Payloads/Payload_Base.h"
#include "GameFramework/Actor.h"


UPayload_Base::UPayload_Base()
{
	EffectTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Effect.Base"));
}

void UPayload_Base::ApplyEffect_Implementation(AActor* Target, const FItemContext& Context)
{
	if (!Target)
	{
		return;
	}
	
	FItemEffectSpec Spec;
	Spec.EffectTag = EffectTag.IsValid() ? EffectTag : FGameplayTag::RequestGameplayTag(TEXT("Item.Effect.Base"), false);
	Spec.Magnitude = Magnitude;
	Spec.Duration = Duration;

	if (Target->Implements<UItemInterface>())
	{
		IItemInterface::Execute_ApplyItemEffect(Target, Spec, Context);
	}
}
