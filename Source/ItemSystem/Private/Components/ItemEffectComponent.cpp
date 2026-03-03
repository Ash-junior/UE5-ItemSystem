#include "Components/ItemEffectComponent.h"
#include "Core/ItemSystemLog.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UItemEffectComponent::UItemEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

FGuid UItemEffectComponent::ApplyEffect(const FItemEffectApplyRequest& Request)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return FGuid();
	}

	UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	const float Duration = Request.Spec.Duration;
	const float EndTime = Duration > 0.0f ? Now + Duration : 0.0f;

	FItemActiveEffect* Existing = nullptr;

	if (Request.StackPolicy == EItemEffectStackPolicy::RefreshDuration)
	{
		Existing = ActiveEffects.FindByPredicate([&Request](const FItemActiveEffect& E)
		{
			return E.EffectTag == Request.Spec.EffectTag && E.EffectChannel == Request.EffectChannel;
		});
	}
	else if (Request.StackPolicy == EItemEffectStackPolicy::ReplaceBySource && Request.SourceKey != NAME_None)
	{
		Existing = ActiveEffects.FindByPredicate([&Request](const FItemActiveEffect& E)
		{
			return E.SourceKey == Request.SourceKey && E.EffectChannel == Request.EffectChannel;
		});
	}

	FGuid EffectId;

	if (Existing)
	{
		EffectId = Existing->EffectId;
		Existing->Magnitude = Request.Spec.Magnitude;
		Existing->Duration = Duration;
		Existing->StartTime = Now;
		Existing->EndTime = EndTime;
		Existing->Aggregation = Request.Aggregation;
		Existing->SourceKey = Request.SourceKey;
	}
	else
	{
		FItemActiveEffect NewEffect;
		NewEffect.EffectId = FGuid::NewGuid();
		NewEffect.EffectTag = Request.Spec.EffectTag;
		NewEffect.EffectChannel = Request.EffectChannel;
		NewEffect.Magnitude = Request.Spec.Magnitude;
		NewEffect.Duration = Duration;
		NewEffect.StartTime = Now;
		NewEffect.EndTime = EndTime;
		NewEffect.Aggregation = Request.Aggregation;
		NewEffect.SourceKey = Request.SourceKey;
		ActiveEffects.Add(NewEffect);
		EffectId = NewEffect.EffectId;
	}

	// Refresh timer
	if (FTimerHandle* Handle = EffectTimers.Find(EffectId))
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(*Handle);
		}
	}

	if (Duration > 0.0f && World)
	{
		FTimerHandle& NewHandle = EffectTimers.FindOrAdd(EffectId);
		FTimerDelegate Delegate;
		Delegate.BindUObject(this, &UItemEffectComponent::RemoveEffectInternal, EffectId);
		World->GetTimerManager().SetTimer(NewHandle, Delegate, Duration, false);
	}
	else
	{
		EffectTimers.Remove(EffectId);
	}

	BroadcastEffectsChanged();
	if (IsItemSystemQAEnabled())
	{
		UE_LOG(LogItemSystem, Log, TEXT("QA: Effect applied %s (Channel: %s, Magnitude: %.2f, Duration: %.2f, Id: %s)"),
			*Request.Spec.EffectTag.ToString(),
			*Request.EffectChannel.ToString(),
			Request.Spec.Magnitude,
			Request.Spec.Duration,
			*EffectId.ToString());
	}

	return EffectId;
}

void UItemEffectComponent::AddOrRefreshEffect(const FItemEffectSpec& Spec)
{
	FItemEffectApplyRequest Request;
	Request.Spec = Spec;
	Request.EffectChannel = NAME_None;
	Request.StackPolicy = EItemEffectStackPolicy::RefreshDuration;
	Request.Aggregation = EItemEffectAggregation::Multiply;
	Request.SourceKey = NAME_None;
	ApplyEffect(Request);
}

void UItemEffectComponent::RemoveEffectByTag(FGameplayTag Tag)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	TArray<FGuid> ToRemove;
	for (const FItemActiveEffect& Effect : ActiveEffects)
	{
		// MatchesTag covers child tags: removing "Item.Effect.ModifySpeed" also removes
		// "Item.Effect.ModifySpeed.Boost", etc.
		if (Effect.EffectTag.MatchesTag(Tag))
		{
			ToRemove.Add(Effect.EffectId);
		}
	}

	for (const FGuid& EffectId : ToRemove)
	{
		RemoveEffectInternal(EffectId);
	}
}

bool UItemEffectComponent::RemoveEffectById(FGuid EffectId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	const int32 BeforeCount = ActiveEffects.Num();
	RemoveEffectInternal(EffectId);
	return ActiveEffects.Num() < BeforeCount;
}

int32 UItemEffectComponent::RemoveEffectsByChannel(FName EffectChannel)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return 0;
	}

	TArray<FGuid> ToRemove;
	for (const FItemActiveEffect& Effect : ActiveEffects)
	{
		if (Effect.EffectChannel == EffectChannel)
		{
			ToRemove.Add(Effect.EffectId);
		}
	}

	for (const FGuid& EffectId : ToRemove)
	{
		RemoveEffectInternal(EffectId);
	}

	return ToRemove.Num();
}

void UItemEffectComponent::RemoveEffectInternal(FGuid EffectId)
{
	FItemActiveEffect RemovedEffect;
	const int32 RemovedCount = ActiveEffects.RemoveAll([&EffectId, &RemovedEffect](const FItemActiveEffect& E)
	{
		if (E.EffectId == EffectId)
		{
			RemovedEffect = E;
			return true;
		}
		return false;
	});

	if (RemovedCount == 0)
	{
		return;
	}

	if (FTimerHandle* Handle = EffectTimers.Find(EffectId))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(*Handle);
		}
		EffectTimers.Remove(EffectId);
	}

	BroadcastEffectsChanged();
	if (IsItemSystemQAEnabled())
	{
		UE_LOG(LogItemSystem, Log, TEXT("QA: Effect removed %s (Channel: %s, Id: %s)"),
			*RemovedEffect.EffectTag.ToString(),
			*RemovedEffect.EffectChannel.ToString(),
			*EffectId.ToString());
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
