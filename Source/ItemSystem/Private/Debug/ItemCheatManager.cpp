#include "Debug/ItemCheatManager.h"
#include "Core/ItemSystemManager.h"
#include "Core/InventoryComponent.h"
#include "Data/ItemDefinition.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"

void UItemCheatManager::Cheat_GiveItem(FString TagQueryString)
{
    // 1. Get the Player Pawn
    APawn* MyPawn = GetPlayerController() ? GetPlayerController()->GetPawn() : nullptr;
    if (!MyPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: No Pawn found!"));
        return;
    }

    // 2. Find Inventory Component
    UInventoryComponent* Inventory = MyPawn->FindComponentByClass<UInventoryComponent>();
    if (!Inventory)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: Pawn has no Inventory Component!"));
        return;
    }

    // 3. Find System Manager
    UItemSystemManager* Manager = UItemSystemManager::Get(this);
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("Cheat: ItemSystemManager is missing from GameState!"));
        return;
    }

    // 4. Create a Tag Query from the string
    // This allows typing "Item.Type.Offense" in the console
    FGameplayTagQuery Query = FGameplayTagQuery::MakeQuery_MatchTag(FGameplayTag::RequestGameplayTag(FName(*TagQueryString)));

    if (Query.IsEmpty())
    {
         UE_LOG(LogTemp, Warning, TEXT("Cheat: Invalid Tag Query or Tag not found."));
         return;
    }

    // 5. Get Item and Grant it
    UItemDefinition* FoundItem = Manager->GetItemByQuery(Query);
    if (FoundItem)
    {
        Inventory->Server_GrantItem(FoundItem, 1);
        UE_LOG(LogTemp, Log, TEXT("Cheat: Granted item %s"), *FoundItem->GetName());
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Cheat: No item found matching query: %s"), *TagQueryString);
    }
}

void UItemCheatManager::Cheat_ClearInventory()
{
    // Implementation left as an exercise (set CurrentItem to nullptr in Inventory)
    // For now, simple log.
    UE_LOG(LogTemp, Log, TEXT("Cheat: Clear Inventory not fully implemented yet."));
}

