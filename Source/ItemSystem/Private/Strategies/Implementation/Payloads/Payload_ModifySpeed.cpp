#include "Strategies/Implementation/Payloads/Payload_ModifySpeed.h"

#include "Core/ItemInterface.h"
#include "GameplayTagsManager.h"
#include "GameFramework/Actor.h"

UPayload_ModifySpeed::UPayload_ModifySpeed()
{
	EffectTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Effect.ModifySpeed"), false);
}

void UPayload_ModifySpeed::ApplyEffect_Implementation(AActor* Target, const FItemContext& Context)
{
	if (!Target)
	{
		return;
	}

	FItemEffectSpec Spec;
	Spec.EffectTag = EffectTag.IsValid() ? EffectTag : FGameplayTag::RequestGameplayTag(TEXT("Item.Effect.ModifySpeed"), false);
	Spec.Magnitude = SpeedMultiplier;
	Spec.Duration = Duration;

	if (Target->Implements<UItemInterface>())
	{
		IItemInterface::Execute_ApplyItemEffect(Target, Spec, Context);
	}
}
