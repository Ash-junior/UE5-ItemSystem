#include "UI/ItemEffectBlueprintLibrary.h"
#include "Components/ItemEffectComponent.h"

// ---------------------------------------------------------------------------
// Effect progress

float UItemEffectBlueprintLibrary::GetEffectNormalizedProgress(const FItemActiveEffect& Effect, float CurrentTime)
{
	return Effect.GetNormalizedProgress(CurrentTime);
}

float UItemEffectBlueprintLibrary::GetEffectRemainingTime(const FItemActiveEffect& Effect, float CurrentTime)
{
	return Effect.GetRemainingTime(CurrentTime);
}

// ---------------------------------------------------------------------------
// Effect queries

TArray<FItemActiveEffect> UItemEffectBlueprintLibrary::GetEffectsByTag(UItemEffectComponent* Component, FGameplayTag Tag)
{
	TArray<FItemActiveEffect> Result;
	if (!Component || !Tag.IsValid())
	{
		return Result;
	}

	for (const FItemActiveEffect& Effect : Component->GetActiveEffects())
	{
		if (Effect.EffectTag.MatchesTag(Tag))
		{
			Result.Add(Effect);
		}
	}

	return Result;
}

bool UItemEffectBlueprintLibrary::HasActiveEffect(UItemEffectComponent* Component, FGameplayTag Tag)
{
	if (!Component || !Tag.IsValid())
	{
		return false;
	}

	for (const FItemActiveEffect& Effect : Component->GetActiveEffects())
	{
		if (Effect.EffectTag.MatchesTag(Tag))
		{
			return true;
		}
	}

	return false;
}
