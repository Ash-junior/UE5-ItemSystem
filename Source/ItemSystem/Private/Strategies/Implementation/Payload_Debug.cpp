#include "Strategies/Implementation/Payload_Debug.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"

void UPayload_Debug::ApplyEffect_Implementation(AActor* Target, const FItemContext& Context)
{
	FString TargetName = Target ? Target->GetName() : TEXT("NULL");
    
	// Print to screen
	if (GEngine)
	{
		FString FinalMsg = FString::Printf(TEXT("%s | Target: %s"), *DebugMessage, *TargetName);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, TextColor, FinalMsg);
	}

	// Print to log
	UE_LOG(LogTemp, Warning, TEXT("Payload_Debug: %s (Hit: %s)"), *DebugMessage, *TargetName);
}


