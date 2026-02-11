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
    }

    // Force update on server side too
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
    Manager->SpawnItemExecution(Context);

    // 4. Handle Ammo Consumption
    CurrentAmmo--;
    
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

    // Check Blocking Tags (e.g., is the player Stunned?)
    if (CurrentItem && !CurrentItem->UsageBlockingTags.IsEmpty())
    {
        // Iterate blocking tags and ask interface if owner has them
        // Note: Ideally, IItemInterface should return a TagContainer to compare against.
        // For now, we assume simple checks.
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

