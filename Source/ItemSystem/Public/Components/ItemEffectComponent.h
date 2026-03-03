#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ItemSystemTypes.h"
#include "GameplayTagContainer.h"
#include "ItemEffectComponent.generated.h"

UENUM(BlueprintType)
enum class EItemEffectStackPolicy : uint8
{
	RefreshDuration UMETA(DisplayName = "Refresh Duration"),
	ReplaceBySource UMETA(DisplayName = "Replace By Source"),
	StackIndependent UMETA(DisplayName = "Stack Independent")
};

UENUM(BlueprintType)
enum class EItemEffectAggregation : uint8
{
	Multiply UMETA(DisplayName = "Multiply"),
	Add UMETA(DisplayName = "Add")
};

USTRUCT(BlueprintType)
struct FItemEffectApplyRequest
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FItemEffectSpec Spec;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FItemContext Context;

	// Logical channel used by effect handlers (example: Movement.Speed).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName EffectChannel = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	EItemEffectStackPolicy StackPolicy = EItemEffectStackPolicy::RefreshDuration;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	EItemEffectAggregation Aggregation = EItemEffectAggregation::Multiply;

	// Optional source key (used by ReplaceBySource policy).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	FName SourceKey = NAME_None;
};

USTRUCT(BlueprintType)
struct FItemActiveEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FGuid EffectId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FName EffectChannel = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float Magnitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float StartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float EndTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	EItemEffectAggregation Aggregation = EItemEffectAggregation::Multiply;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FName SourceKey = NAME_None;

	// ------------------------------------------------------------------ UI helpers (C++ only — use UItemEffectBlueprintLibrary for BP access)

	// Returns how far along the effect is, from 0 (just applied) to 1 (expired).
	// Returns 1 for permanent effects (Duration <= 0).
	float GetNormalizedProgress(float CurrentTime) const
	{
		if (Duration <= 0.0f || EndTime <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::Clamp((CurrentTime - StartTime) / Duration, 0.0f, 1.0f);
	}

	// Returns seconds remaining before the effect expires.
	// Returns -1 for permanent effects (Duration <= 0).
	float GetRemainingTime(float CurrentTime) const
	{
		if (EndTime <= 0.0f)
		{
			return -1.0f;
		}
		return FMath::Max(0.0f, EndTime - CurrentTime);
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnItemEffectsChanged);

/**
 * Tracks active item effects for UI/feedback. Replicated to clients.
 */
UCLASS(ClassGroup = (ItemSystem), meta = (BlueprintSpawnableComponent))
class ITEMSYSTEM_API UItemEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemEffectComponent();

	UPROPERTY(BlueprintAssignable, Category = "Item System")
	FOnItemEffectsChanged OnEffectsChanged;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item System")
	FGuid ApplyEffect(const FItemEffectApplyRequest& Request);

	// Legacy helper kept for compatibility.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item System")
	void AddOrRefreshEffect(const FItemEffectSpec& Spec);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item System")
	void RemoveEffectByTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item System")
	bool RemoveEffectById(FGuid EffectId);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Item System")
	int32 RemoveEffectsByChannel(FName EffectChannel);

	UFUNCTION(BlueprintPure, Category = "Item System")
	const TArray<FItemActiveEffect>& GetActiveEffects() const { return ActiveEffects; }

protected:
	UPROPERTY(ReplicatedUsing = OnRep_ActiveEffects)
	TArray<FItemActiveEffect> ActiveEffects;

	UFUNCTION()
	void OnRep_ActiveEffects();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	TMap<FGuid, FTimerHandle> EffectTimers;

	void BroadcastEffectsChanged();
	void RemoveEffectInternal(FGuid EffectId);
};
