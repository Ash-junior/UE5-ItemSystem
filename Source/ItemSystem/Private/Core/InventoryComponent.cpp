#include "Core/InventoryComponent.h"
#include "Data/ItemDefinition.h"
#include "Core/ItemSystemManager.h"
#include "Core/ItemInterface.h"
#include "Net/UnrealNetwork.h"
#include "Engine/ActorChannel.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/GameStateBase.h"

UInventoryComponent::UInventoryComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    bWantsInitializeComponent = true;
    SetIsReplicatedByDefault(true);

    // Default values
    CurrentAmmo = 0;
    CurrentItem = nullptr;
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // Replicate the Item Definition to everyone (so they can see the mesh)
    DOREPLIFETIME(UInventoryComponent, CurrentItem);
    
    // Replicate Ammo only to the owner (others don't need to know exact count)
    DOREPLIFETIME_CONDITION(UInventoryComponent, CurrentAmmo, COND_OwnerOnly);
}

void UInventoryComponent::BeginPlay()
{
    Super::BeginPlay();

    // Create the mesh component dynamically if it doesn't exist
    if (GetOwner())
    {
        HeldMeshComponent = NewObject<UStaticMeshComponent>(GetOwner(), TEXT("ItemHeldMesh"));
        HeldMeshComponent->RegisterComponent();
        HeldMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeldMeshComponent->SetHiddenInGame(true);
    }
}

void UInventoryComponent::OnRep_CurrentItem()
{
    // When the server updates the item, update the visuals on the client
    UpdateVisuals(CurrentItem != nullptr);
    
    // Notify UI
    if (OnInventoryChanged.IsBound())
    {
        OnInventoryChanged.Broadcast(CurrentItem, CurrentAmmo);
    }
}

void UInventoryComponent::Server_GrantItem_Implementation(UItemDefinition* NewItem, int32 Amount)
{
    if (!NewItem) return;

    // Logic: If we already have this item, stack it. If different, replace it.
    if (CurrentItem == NewItem)
    {
        CurrentAmmo = FMath::Min<int32>(CurrentAmmo + Amount, NewItem->MaxStack);
    }
    else
    {
        CurrentItem = NewItem;
        CurrentAmmo = FMath::Clamp<int32>(Amount, 1, NewItem->MaxStack);
        LastActivationTime = -FLT_MAX;
    }

    // Force update on server side too
    OnRep_CurrentItem();
}

void UInventoryComponent::Server_ClearInventory_Implementation()
{
    CurrentItem = nullptr;
    CurrentAmmo = 0;
    LastActivationTime = -FLT_MAX;
    OnRep_CurrentItem();
}

void UInventoryComponent::Server_TryActivateItem_Implementation()
{
    // 1. Validation Checks
    if (!CurrentItem || CurrentAmmo <= 0) return;
    if (!CanUseItem()) return;

    // 2. Find the System Manager (Singleton-like access)
    UItemSystemManager* Manager = UItemSystemManager::Get(this);
    if (!Manager)
    {
        UE_LOG(LogTemp, Error, TEXT("Inventory: ItemSystemManager not found in GameState!"));
        return;
    }

    // 3. Create Context and Spawn
    FItemContext Context = MakeItemContext();
    AItemExecutionStrategy* NewActor = Manager->SpawnItemExecution(Context);
    if (!NewActor)
    {
        UE_LOG(LogTemp, Warning, TEXT("Inventory: Failed to spawn execution for item %s"), *CurrentItem->GetName());
        return;
    }

    // 4. Handle Ammo Consumption
    CurrentAmmo--;
    LastActivationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastActivationTime;
    
    if (CurrentAmmo <= 0)
    {
        CurrentItem = nullptr; // Item depleted
    }

    // 5. Update UI/Visuals
    OnRep_CurrentItem();
}

bool UInventoryComponent::CanUseItem() const
{
    // Basic check. In the future, check Cooldowns here too.
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !OwnerActor->Implements<UItemInterface>()) return false;

    if (!CurrentItem)
    {
        return false;
    }

    // Check Cooldown
    if (CurrentItem->Cooldown > 0.0f)
    {
        const UWorld* World = GetWorld();
        const float Now = World ? World->GetTimeSeconds() : 0.0f;
        if ((Now - LastActivationTime) < CurrentItem->Cooldown)
        {
            return false;
        }
    }

    // Check Blocking Tags (e.g., is the player Stunned?)
    if (!CurrentItem->UsageBlockingTags.IsEmpty())
    {
        for (const FGameplayTag& Tag : CurrentItem->UsageBlockingTags)
        {
            if (IItemInterface::Execute_HasGameplayTag(OwnerActor, Tag))
            {
                return false;
            }
        }
    }

    return true;
}

FItemContext UInventoryComponent::MakeItemContext() const
{
    FItemContext Context;
    Context.Instigator = GetOwner();
    Context.ItemDefinition = CurrentItem;
    Context.RandomSeed = FMath::Rand();
    Context.InvocationGUID = FGuid::NewGuid();
    
    if (CurrentItem)
    {
        Context.ContextTags = CurrentItem->IdentityTags;
    }
    
    // Get Controller info via Interface
    if (GetOwner()->Implements<UItemInterface>())
    {
        Context.InstigatorController = IItemInterface::Execute_GetItemInstigatorController(GetOwner());
    }

    // Set Origin (approximate to Actor location, ideally should be a muzzle socket)
    Context.OriginTransform = GetOwner()->GetActorTransform();
    if (CurrentItem && CurrentItem->AttachSocketTag.IsValid() && GetOwner()->Implements<UItemInterface>())
    {
        FName SocketName;
        USceneComponent* ParentComp = IItemInterface::Execute_GetSocketByTag(GetOwner(), CurrentItem->AttachSocketTag, SocketName);
        if (ParentComp)
        {
            if (SocketName != NAME_None && ParentComp->DoesSocketExist(SocketName))
            {
                Context.OriginTransform = ParentComp->GetSocketTransform(SocketName, RTS_World);
            }
            else
            {
                Context.OriginTransform = ParentComp->GetComponentTransform();
            }
        }
    }

    return Context;
}

void UInventoryComponent::UpdateVisuals(bool bVisible)
{
    if (!HeldMeshComponent) return;

    if (!bVisible || !CurrentItem)
    {
        HeldMeshComponent->SetHiddenInGame(true);
        return;
    }

    // 1. Set the Mesh
    if (CurrentItem->Visuals.HeldMesh)
    {
        HeldMeshComponent->SetStaticMesh(CurrentItem->Visuals.HeldMesh);
        HeldMeshComponent->SetHiddenInGame(false);
    }

    // 2. Attach to the correct socket using the Interface
    AActor* OwnerActor = GetOwner();
    if (OwnerActor && OwnerActor->Implements<UItemInterface>())
    {
        FName SocketName;
        // Convert the abstract Tag (e.g. "Mount.Roof") to a real Socket Name via the Pawn's logic
        USceneComponent* ParentComp = IItemInterface::Execute_GetSocketByTag(OwnerActor, CurrentItem->AttachSocketTag, SocketName);

        if (ParentComp)
        {
            HeldMeshComponent->AttachToComponent(ParentComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketName);
        }
    }
}

