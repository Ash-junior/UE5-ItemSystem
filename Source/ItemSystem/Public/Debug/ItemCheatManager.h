#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "ItemCheatManager.generated.h"

/**
 * Debug tools to test the Item System.
 * Commands can be executed via the console (~) during play.
 */
UCLASS()
class ITEMSYSTEM_API UItemCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	/**
	 * Gives an item to the player by finding an item that matches the tag query.
	 * Command: Cheat_GiveItem "Tag.To.Search"
	 * Command: Cheat_GiveItem "Tag.To.Search" 3
	 */
	UFUNCTION(Exec)
	void Cheat_GiveItem(FString TagQueryString, int32 Amount = 1);

	/**
	 * Forces the inventory to clear.
	 */
	UFUNCTION(Exec)
	void Cheat_ClearInventory();

	/**
	 * Simulate an impact by applying the payload matching a tag query to the hit actor.
	 * Command: Cheat_SimulateImpact "Tag.Query"
	 */
	UFUNCTION(Exec)
	void Cheat_SimulateImpact(FString TagQueryString);
};
