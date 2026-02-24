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
		ApplySpeedEffect(Spec.Magnitude, Spec.Duration, Spec.EffectTag);

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

	// Capture base speed only when no effect is currently active
	if (!bHasBaseSpeed)
	{
		BaseSpeed = MoveComp->MaxWalkSpeed;
		bHasBaseSpeed = true;
	}

	// Register this effect instance with a unique ID
	FActiveSpeedEffect& NewEffect = ActiveSpeedEffects.AddDefaulted_GetRef();
	NewEffect.EffectID = NextSpeedEffectID++;
	NewEffect.Tag = EffectTag;
	NewEffect.Multiplier = Multiplier;

	RecalculateAndApplySpeed();

	if (IsItemSystemQAEnabled())
	{
		UE_LOG(LogItemSystem, Log, TEXT("QA: Speed effect applied — tag: %s, multiplier: %.2f, duration: %.2f, base: %.1f -> new: %.1f"),
			*EffectTag.ToString(), Multiplier, Duration, BaseSpeed, MoveComp->MaxWalkSpeed);
	}

	if (Duration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerDelegate Delegate;
			Delegate.BindUObject(this, &UItemEffectHandlerComponent::OnSpeedEffectExpiredByID, NewEffect.EffectID);
			World->GetTimerManager().SetTimer(NewEffect.TimerHandle, Delegate, Duration, false);
		}
	}
}

void UItemEffectHandlerComponent::RecalculateAndApplySpeed()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character || !bHasBaseSpeed)
	{
		return;
	}

	UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	float Combined = 1.0f;
	for (const FActiveSpeedEffect& Effect : ActiveSpeedEffects)
	{
		Combined *= Effect.Multiplier;
	}

	MoveComp->MaxWalkSpeed = BaseSpeed * Combined;
}

void UItemEffectHandlerComponent::OnSpeedEffectExpiredByID(uint32 EffectID)
{
	const int32 Index = ActiveSpeedEffects.IndexOfByPredicate(
		[EffectID](const FActiveSpeedEffect& Effect) { return Effect.EffectID == EffectID; });

	if (Index == INDEX_NONE)
	{
		return;
	}

	ActiveSpeedEffects.RemoveAtSwap(Index, 1, EAllowShrinking::No);

	if (ActiveSpeedEffects.IsEmpty())
	{
		// All effects gone — restore base speed
		ACharacter* Character = Cast<ACharacter>(GetOwner());
		if (Character)
		{
			if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
			{
				MoveComp->MaxWalkSpeed = BaseSpeed;

				if (IsItemSystemQAEnabled())
				{
					UE_LOG(LogItemSystem, Log, TEXT("QA: Speed effect expired — speed restored to %.1f"), BaseSpeed);
				}
			}
		}

		bHasBaseSpeed = false;
		BaseSpeed = 0.0f;

		if (UItemEffectComponent* EffectComp = FindEffectComponent())
		{
			EffectComp->RemoveEffectByTag(SpeedEffectParentTag);
		}
	}
	else
	{
		// Other effects remain — recalculate combined speed
		RecalculateAndApplySpeed();

		if (IsItemSystemQAEnabled())
		{
			ACharacter* Character = Cast<ACharacter>(GetOwner());
			UCharacterMovementComponent* MoveComp = Character ? Character->GetCharacterMovement() : nullptr;
			UE_LOG(LogItemSystem, Log, TEXT("QA: Speed effect expired — %d effect(s) remaining, speed: %.1f"),
				ActiveSpeedEffects.Num(), MoveComp ? MoveComp->MaxWalkSpeed : 0.0f);
		}
	}
}

UItemEffectComponent* UItemEffectHandlerComponent::FindEffectComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UItemEffectComponent>() : nullptr;
}
