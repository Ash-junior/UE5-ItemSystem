#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "Core/ItemSystemTypes.h"
#include "ItemInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface implemented by any Pawn that can use items.
 */
class ITEMSYSTEM_API IItemInterface
{
	GENERATED_BODY()

public:
	/**
	 * Returns the physical component and socket name corresponding to a gameplay tag.
	 * Example: Tag 'Socket.Mount.Roof' -> Returns the MeshComponent and 'RoofSocket' name.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	USceneComponent* GetSocketByTag(FGameplayTag SocketTag, FName& OutSocketName) const;

	/**
	 * Returns the Team ID of the user. Used to avoid friendly fire.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	int32 GetTeamID() const;

	/**
	 * Returns the controller of the pawn.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	AController* GetItemInstigatorController() const;

	/**
	 * Checks if the user has a specific state (e.g., State.Status.Stunned).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	bool HasGameplayTag(FGameplayTag TagToCheck) const;

	/**
	 * Receives a generic item effect. Return true if handled.
	 * Useful for modular systems (Mover, GAS, custom movement).
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	bool ApplyItemEffect(const FItemEffectSpec& Effect, const FItemContext& Context);

	virtual bool ApplyItemEffect_Implementation(const FItemEffectSpec& Effect, const FItemContext& Context) { return false; }
	
	/**
	 * Add a gameplay tag to pawn tags container
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	bool AddStatusTag(FGameplayTag TagToAdd);
	
	/**
	 * Remove a gameplay tag from pawn tags container
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Item System")
	bool RemoveStatusTag(FGameplayTag TagToRemove);
};
