#include "Strategies/Implementation/Payloads/Payload_ModifySpeed.h"

#include "Components/ItemSpeedModifierComponent.h"
#include "Core/ItemInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagsManager.h"

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

	FGameplayTag TagToUse = EffectTag;
	if (!TagToUse.IsValid())
	{
		TagToUse = FGameplayTag::RequestGameplayTag(TEXT("Item.Effect.ModifySpeed"), false);
	}

	FItemEffectSpec Effect;
	Effect.EffectTag = TagToUse;
	Effect.Magnitude = SpeedMultiplier;
	Effect.Duration = Duration;

	if (Target->Implements<UItemInterface>())
	{
		if (IItemInterface::Execute_ApplyItemEffect(Target, Effect, Context))
		{
			return;
		}
	}

	ACharacter* Character = Cast<ACharacter>(Target);
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	UItemSpeedModifierComponent* ModifierComp = Target->FindComponentByClass<UItemSpeedModifierComponent>();
	if (!ModifierComp)
	{
		ModifierComp = NewObject<UItemSpeedModifierComponent>(Target);
		ModifierComp->RegisterComponent();
	}

	ModifierComp->ApplySpeedModifier(MoveComp, TagToUse, SpeedMultiplier, Duration);
}
