#include "Components/ItemAimPreviewComponent.h"

#include "Core/InventoryComponent.h"
#include "Core/ItemInterface.h"
#include "Core/ItemProjectileTrajectoryLibrary.h"
#include "Data/ItemDefinition.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UItemAimPreviewComponent::UItemAimPreviewComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UItemAimPreviewComponent::BeginPlay()
{
    Super::BeginPlay();
    InventoryComponent = ResolveInventoryComponent(nullptr);
}

void UItemAimPreviewComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsAiming)
    {
        UpdateAimPreview();
    }
}

void UItemAimPreviewComponent::StartAiming(UInventoryComponent* InventoryOverride)
{
    InventoryComponent = ResolveInventoryComponent(InventoryOverride);
    bIsAiming = true;
    SetComponentTickEnabled(true);
    UpdateAimPreview();
}

void UItemAimPreviewComponent::StopAiming()
{
    bIsAiming = false;
    SetComponentTickEnabled(false);

    LastAimData = FItemAimData();
    LastArcResult = FItemProjectileArcResult();

    OnAimPreviewStopped.Broadcast();
}

bool UItemAimPreviewComponent::UpdateAimPreview()
{
    UInventoryComponent* Inventory = ResolveInventoryComponent(InventoryComponent);
    AActor* OwnerActor = GetOwner();
    if (!Inventory || !OwnerActor)
    {
        LastAimData = FItemAimData();
        LastArcResult = FItemProjectileArcResult();
        return false;
    }

    UItemDefinition* CurrentItem = Inventory->GetCurrentItem();
    if (!CurrentItem)
    {
        LastAimData = FItemAimData();
        LastArcResult = FItemProjectileArcResult();
        return false;
    }

    FItemProjectileArcParams Params;
    if (!UItemProjectileTrajectoryLibrary::BuildArcParamsForItem(this, OwnerActor, ResolveInstigatorController(), CurrentItem, Params))
    {
        LastAimData = FItemAimData();
        LastArcResult = FItemProjectileArcResult();
        return false;
    }

    Params.bTraceWithCollision = bTraceWithCollision;
    Params.MaxSimTime = MaxSimTime;
    Params.SimFrequency = SimFrequency;
    Params.TraceChannel = TraceChannel;

    if (!UItemProjectileTrajectoryLibrary::ComputeProjectileArc(this, Params, LastArcResult))
    {
        LastAimData = FItemAimData();
        LastArcResult = FItemProjectileArcResult();
        return false;
    }

    LastAimData = UItemProjectileTrajectoryLibrary::MakeAimDataFromArc(Params, LastArcResult);

    if (bDrawDebugPreview)
    {
        DrawDebugArc();
    }

    OnAimPreviewUpdated.Broadcast(LastArcResult, LastAimData);
    return LastAimData.bIsValid;
}

UInventoryComponent* UItemAimPreviewComponent::ResolveInventoryComponent(UInventoryComponent* InventoryOverride) const
{
    if (InventoryOverride)
    {
        return InventoryOverride;
    }

    if (InventoryComponent)
    {
        return InventoryComponent;
    }

    AActor* OwnerActor = GetOwner();
    return OwnerActor ? OwnerActor->FindComponentByClass<UInventoryComponent>() : nullptr;
}

AController* UItemAimPreviewComponent::ResolveInstigatorController() const
{
    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return nullptr;
    }

    if (OwnerActor->Implements<UItemInterface>())
    {
        if (AController* InterfaceController = IItemInterface::Execute_GetItemInstigatorController(OwnerActor))
        {
            return InterfaceController;
        }
    }

    if (const APawn* Pawn = Cast<APawn>(OwnerActor))
    {
        return Pawn->GetController();
    }

    return nullptr;
}

void UItemAimPreviewComponent::DrawDebugArc() const
{
    UWorld* World = GetWorld();
    if (!World || LastArcResult.PathPoints.Num() < 2)
    {
        return;
    }

    for (int32 Index = 1; Index < LastArcResult.PathPoints.Num(); ++Index)
    {
        DrawDebugLine(
            World,
            LastArcResult.PathPoints[Index - 1],
            LastArcResult.PathPoints[Index],
            FColor::Cyan,
            false,
            0.0f,
            0,
            3.0f);
    }

    if (LastArcResult.bHit)
    {
        DrawDebugSphere(World, LastArcResult.TracedPosition, 18.0f, 12, FColor::Green, false, 0.0f);
        DrawDebugDirectionalArrow(
            World,
            LastArcResult.TracedPosition,
            LastArcResult.TracedPosition + LastArcResult.TracedNormal * 80.0f,
            20.0f,
            FColor::Green,
            false,
            0.0f,
            0,
            2.0f);
    }
}
