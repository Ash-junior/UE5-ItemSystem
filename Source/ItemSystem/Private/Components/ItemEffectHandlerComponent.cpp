#include "Components/ItemEffectHandlerComponent.h"
#include "Components/ItemEffectComponent.h"

UItemEffectHandlerComponent::UItemEffectHandlerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UItemEffectHandlerComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UItemEffectHandlerComponent::HandleEffect_Implementation(const FItemEffectSpec& Spec, const FItemContext& Context)
{
	return false;
}

UItemEffectComponent* UItemEffectHandlerComponent::FindEffectComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UItemEffectComponent>() : nullptr;
}
