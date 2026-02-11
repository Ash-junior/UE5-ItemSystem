#include "Strategies/Implementation/Executions/Execution_Trap.h"

#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Strategies/ItemPayloadStrategy.h"

AExecution_Trap::AExecution_Trap()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create Trigger Component
	TriggerComponent = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerComponent"));
	TriggerComponent->InitSphereRadius(TriggerRadius);
	TriggerComponent->SetCanEverAffectNavigation(false);
	TriggerComponent->SetGenerateOverlapEvents(true);
	TriggerComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// Bind overlap
	TriggerComponent->OnComponentBeginOverlap.AddDynamic(this, &AExecution_Trap::OnTrapOverlap);

	RootComponent = TriggerComponent;
}

void AExecution_Trap::ResetForReuse()
{
	bHasTriggered = false;
}

void AExecution_Trap::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerComponent)
	{
		TriggerComponent->SetSphereRadius(TriggerRadius);
	}

	if (LifeSpanSeconds > 0.0f)
	{
		SetLifeSpan(LifeSpanSeconds);
	}

	if (ItemContext.Instigator)
	{
		TriggerComponent->MoveIgnoreActors.Add(ItemContext.Instigator);
	}
}

void AExecution_Trap::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugVisuals)
	{
		DrawDebugSphere(GetWorld(), GetActorLocation(), TriggerRadius, 16, FColor::Orange, false, -1.0f, 0, 2.0f);
	}
}

bool AExecution_Trap::CanTriggerOnActor(AActor* OtherActor) const
{
	if (!OtherActor || !OtherActor->IsValidLowLevel() || OtherActor == this)
	{
		return false;
	}

	if (ItemContext.Instigator && OtherActor == ItemContext.Instigator)
	{
		return false;
	}

	if (GetInstigator() && OtherActor == GetInstigator())
	{
		return false;
	}

	if (!ShouldAffectActor(OtherActor))
	{
		return false;
	}

	return true;
}

void AExecution_Trap::TriggerTrap(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FHitResult* Hit)
{
	if (bHasTriggered || !HasAuthority())
	{
		return;
	}

	bHasTriggered = true;

	if (bShowDebugVisuals && GEngine)
	{
		const FString ComponentName = OtherComp ? OtherComp->GetName() : TEXT("None");
		const FString HitMsg = FString::Printf(TEXT("Trap TRIGGERED: %s (Component: %s)"), *OtherActor->GetName(), *ComponentName);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, HitMsg);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *HitMsg);

		FVector ImpactPoint = GetActorLocation();
		if (Hit)
		{
			ImpactPoint = Hit->ImpactPoint;
		}
		else if (OtherComp)
		{
			ImpactPoint = OtherComp->GetComponentLocation();
		}
		DrawDebugPoint(GetWorld(), ImpactPoint, 20.0f, FColor::Cyan, false, 2.0f);
	}

	if (PayloadInstance)
	{
		FItemContext PayloadContext = ItemContext;
		if (Hit)
		{
			PayloadContext.bHasImpactPoint = true;
			PayloadContext.ImpactPoint = Hit->ImpactPoint;
		}
		else if (OtherComp)
		{
			PayloadContext.bHasImpactPoint = true;
			PayloadContext.ImpactPoint = OtherComp->GetComponentLocation();
		}

		PayloadInstance->ApplyEffect(OtherActor, PayloadContext);
	}

	FinishExecution();
}

void AExecution_Trap::OnTrapOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!CanTriggerOnActor(OtherActor))
	{
		return;
	}

	TriggerTrap(OtherActor, OtherComp, &SweepResult);
}
