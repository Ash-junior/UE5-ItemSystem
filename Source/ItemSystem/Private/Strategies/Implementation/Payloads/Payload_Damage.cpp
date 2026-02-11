#include "Strategies/Implementation/Payloads/Payload_Damage.h"

#include "Kismet/GameplayStatics.h"

void UPayload_Damage::ApplyEffect_Implementation(AActor* Target, const FItemContext& Context)
{
	if (!Target)
	{
		return;
	}

	// Apply generic Unreal Engine damage
	// The Instigator (Player/AI) is retrieved from the ItemContext
	UGameplayStatics::ApplyDamage(
		Target,
		DamageAmount,
		Context.InstigatorController, // Event Instigator
		Context.Instigator,           // Damage Causer
		DamageTypeClass               // Damage Type
	);

	// Optional: Log for debug
	UE_LOG(LogTemp, Log, TEXT("Payload_Damage: Applied %f damage to %s"), DamageAmount, *Target->GetName());
}