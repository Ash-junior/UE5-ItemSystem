#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/ItemSystemTypes.h"
#include "ItemEffectHandlerComponent.generated.h"

class UItemEffectComponent;

/**
 * Component attached to a pawn. Entry point for FItemEffectSpec routing.
 *
 * The base C++ implementation returns false for all effects — override
 * HandleEffect in a Blueprint subclass to implement per-project effect types
 * (e.g. speed modification, shields, status effects).
 *
 * Optionally notifies a sibling UItemEffectComponent (for UI tracking).
 */
UCLASS(Blueprintable, ClassGroup = (ItemSystem), meta = (BlueprintSpawnableComponent))
class ITEMSYSTEM_API UItemEffectHandlerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UItemEffectHandlerComponent();

	/**
	 * Main entry point. Called by the pawn's IItemInterface::ApplyItemEffect.
	 * Returns true if the effect tag was recognised and handled.
	 * Override in a Blueprint subclass to add project-specific effect routing.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	bool HandleEffect(const FItemEffectSpec& Spec, const FItemContext& Context);
	virtual bool HandleEffect_Implementation(const FItemEffectSpec& Spec, const FItemContext& Context);

protected:
	virtual void BeginPlay() override;

	UItemEffectComponent* FindEffectComponent() const;
};
