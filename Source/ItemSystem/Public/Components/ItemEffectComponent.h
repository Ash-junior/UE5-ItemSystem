#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ItemSystemTypes.h"
#include "GameplayTagContainer.h"
#include "ItemEffectComponent.generated.h"

USTRUCT(BlueprintType)
struct FItemActiveEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float Magnitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float Duration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float StartTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	float EndTime = 0.0f;
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

	UFUNCTION(BlueprintCallable, Category = "Item System")
	void AddOrRefreshEffect(const FItemEffectSpec& Spec);

	UFUNCTION(BlueprintCallable, Category = "Item System")
	void RemoveEffectByTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Item System")
	const TArray<FItemActiveEffect>& GetActiveEffects() const { return ActiveEffects; }

protected:
	UPROPERTY(ReplicatedUsing = OnRep_ActiveEffects)
	TArray<FItemActiveEffect> ActiveEffects;

	UFUNCTION()
	void OnRep_ActiveEffects();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	TMap<FGameplayTag, FTimerHandle> EffectTimers;

	void BroadcastEffectsChanged();
	void RemoveEffectInternal(const FGameplayTag& Tag);
};
