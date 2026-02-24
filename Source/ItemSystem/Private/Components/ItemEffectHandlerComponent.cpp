#include "Components/ItemEffectHandlerComponent.h"

#include "Components/ItemEffectComponent.h"
#include "Core/ItemSystemLog.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTagsManager.h"

UItemEffectHandlerComponent::UItemEffectHandlerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SpeedEffectParentTag = FGameplayTag::RequestGameplayTag(TEXT("Item.Effect.ModifySpeed"), false);
}

void UItemEffectHandlerComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UItemEffectHandlerComponent::HandleEffect(const FItemEffectSpec& Spec, const FItemContext& Context)
{
	if (!Spec.EffectTag.IsValid())
	{
		return false;
	}

	if (SpeedEffectParentTag.IsValid() && Spec.EffectTag.MatchesTag(SpeedEffectParentTag))
	{
		Execute_ApplySpeedEffect(this, Spec.Magnitude, Spec.Duration, Spec.EffectTag);

		// Notify UI component if present
		if (UItemEffectComponent* EffectComp = FindEffectComponent())
		{
			EffectComp->AddOrRefreshEffect(Spec);
		}

		return true;
	}

	return false;
}

void UItemEffectHandlerComponent::ApplySpeedEffect_Implementation(float Multiplier, float Duration, FGameplayTag EffectTag)
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	// Capture base speed only on the first application (no active effect running)
	if (!bHasBaseSpeed)
	{
		BaseSpeed = MoveComp->MaxWalkSpeed;
		bHasBaseSpeed = true;
	}

	MoveComp->MaxWalkSpeed = BaseSpeed * Multiplier;

	if (IsItemSystemQAEnabled())
	{
		UE_LOG(LogItemSystem, Log, TEXT("QA: Speed effect applied — tag: %s, multiplier: %.2f, duration: %.2f, base: %.1f -> new: %.1f"),
			*EffectTag.ToString(), Multiplier, Duration, BaseSpeed, MoveComp->MaxWalkSpeed);
	}

	if (Duration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				SpeedTimerHandle,
				this,
				&UItemEffectHandlerComponent::OnSpeedEffectExpired,
				Duration,
				false);
		}
	}
}

void UItemEffectHandlerComponent::OnSpeedEffectExpired()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (MoveComp && bHasBaseSpeed)
	{
		MoveComp->MaxWalkSpeed = BaseSpeed;

		if (IsItemSystemQAEnabled())
		{
			UE_LOG(LogItemSystem, Log, TEXT("QA: Speed effect expired — speed restored to %.1f"), BaseSpeed);
		}
	}

	bHasBaseSpeed = false;
	BaseSpeed = 0.0f;

	// Clear UI
	if (UItemEffectComponent* EffectComp = FindEffectComponent())
	{
		EffectComp->RemoveEffectByTag(SpeedEffectParentTag);
	}
}

UItemEffectComponent* UItemEffectHandlerComponent::FindEffectComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UItemEffectComponent>() : nullptr;
}