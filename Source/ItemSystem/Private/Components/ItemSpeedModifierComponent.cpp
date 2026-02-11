#include "Components/ItemSpeedModifierComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

void UItemSpeedModifierComponent::ApplySpeedModifier(UCharacterMovementComponent* MoveComp, FGameplayTag Tag, float Multiplier, float Duration)
{
	if (!MoveComp)
	{
		return;
	}

	if (!bHasBaseSpeed)
	{
		BaseSpeed = MoveComp->MaxWalkSpeed;
		bHasBaseSpeed = true;
	}

	FSpeedModifierEntry& Entry = ActiveModifiers.FindOrAdd(Tag);
	Entry.Multiplier = Multiplier;

	if (Entry.TimerHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(Entry.TimerHandle);
		}
	}

	if (Duration > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			FTimerDelegate ClearDelegate;
			ClearDelegate.BindUObject(this, &UItemSpeedModifierComponent::ClearModifier, Tag);
			World->GetTimerManager().SetTimer(Entry.TimerHandle, ClearDelegate, Duration, false);
		}
	}
	else
	{
		Entry.TimerHandle.Invalidate();
	}

	RecomputeSpeed(MoveComp);
}

UCharacterMovementComponent* UItemSpeedModifierComponent::GetMoveComp() const
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	return Character ? Character->GetCharacterMovement() : nullptr;
}

void UItemSpeedModifierComponent::ClearModifier(FGameplayTag Tag)
{
	ActiveModifiers.Remove(Tag);

	UCharacterMovementComponent* MoveComp = GetMoveComp();
	if (!MoveComp)
	{
		return;
	}

	RecomputeSpeed(MoveComp);
}

void UItemSpeedModifierComponent::RecomputeSpeed(UCharacterMovementComponent* MoveComp)
{
	if (!MoveComp)
	{
		return;
	}

	if (ActiveModifiers.Num() == 0)
	{
		MoveComp->MaxWalkSpeed = BaseSpeed;
		BaseSpeed = 0.0f;
		bHasBaseSpeed = false;
		return;
	}

	float CombinedMultiplier = 1.0f;
	for (const TPair<FGameplayTag, FSpeedModifierEntry>& Pair : ActiveModifiers)
	{
		CombinedMultiplier *= Pair.Value.Multiplier;
	}

	MoveComp->MaxWalkSpeed = BaseSpeed * CombinedMultiplier;
}
