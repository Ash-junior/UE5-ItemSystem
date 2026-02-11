#include "Components/ItemEffectComponent.h"

#include "Core/ItemSystemLog.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UItemEffectComponent::UItemEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UItemEffectComponent::AddOrRefreshEffect(const FItemEffectSpec& Spec)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	const float Duration = Spec.Duration;
	const float EndTime = Duration > 0.0f ? Now + Duration : 0.0f;

	FItemActiveEffect* Existing = ActiveEffects.FindByPredicate([&Spec](const FItemActiveEffect& E)
	{
		return E.EffectTag == Spec.EffectTag;
	});

	if (Existing)
	{
		Existing->Magnitude = Spec.Magnitude;
		Existing->Duration = Duration;
		Existing->StartTime = Now;
		Existing->EndTime = EndTime;
	}
	else
	{
		FItemActiveEffect NewEffect;
		NewEffect.EffectTag = Spec.EffectTag;
		NewEffect.Magnitude = Spec.Magnitude;
		NewEffect.Duration = Duration;
		NewEffect.StartTime = Now;
		NewEffect.EndTime = EndTime;
		ActiveEffects.Add(NewEffect);
	}

	// Refresh timer
	if (FTimerHandle* Handle = EffectTimers.Find(Spec.EffectTag))
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(*Handle);
		}
	}

	if (Duration > 0.0f && World)
	{
		FTimerHandle& NewHandle = EffectTimers.FindOrAdd(Spec.EffectTag);
		FTimerDelegate Delegate;
		Delegate.BindUObject(this, &UItemEffectComponent::RemoveEffectInternal, Spec.EffectTag);
		World->GetTimerManager().SetTimer(NewHandle, Delegate, Duration, false);
	}

	BroadcastEffectsChanged();
	if (IsItemSystemQAEnabled())
	{
		UE_LOG(LogItemSystem, Log, TEXT("QA: Effect added/refreshed %s (Magnitude: %.2f, Duration: %.2f)"),
			*Spec.EffectTag.ToString(), Spec.Magnitude, Spec.Duration);
	}
}

void UItemEffectComponent::RemoveEffectByTag(FGameplayTag Tag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	RemoveEffectInternal(Tag);
}

void UItemEffectComponent::RemoveEffectInternal(const FGameplayTag& Tag)
{
	ActiveEffects.RemoveAll([&Tag](const FItemActiveEffect& E)
	{
		return E.EffectTag == Tag;
	});

	if (FTimerHandle* Handle = EffectTimers.Find(Tag))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*Handle);
		}
		EffectTimers.Remove(Tag);
	}

	BroadcastEffectsChanged();
	if (IsItemSystemQAEnabled())
	{
		UE_LOG(LogItemSystem, Log, TEXT("QA: Effect removed %s"), *Tag.ToString());
	}
}

void UItemEffectComponent::OnRep_ActiveEffects()
{
	BroadcastEffectsChanged();
}

void UItemEffectComponent::BroadcastEffectsChanged()
{
	if (OnEffectsChanged.IsBound())
	{
		OnEffectsChanged.Broadcast();
	}
}

void UItemEffectComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UItemEffectComponent, ActiveEffects);
}
