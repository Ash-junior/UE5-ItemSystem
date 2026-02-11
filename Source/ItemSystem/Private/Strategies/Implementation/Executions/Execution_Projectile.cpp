#include "Strategies/Implementation/Executions/Execution_Projectile.h"

#include "Components/SphereComponent.h"
#include "Strategies/ItemPayloadStrategy.h"
#include "Strategies/ItemTargetingStrategy.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"


AExecution_Projectile::AExecution_Projectile()
{
	PrimaryActorTick.bCanEverTick = true;

	// Create Collision Component
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
	CollisionComponent->InitSphereRadius(15.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile")); // Ensure Projectile profile exists in project settings, or use BlockAllDynamic
	CollisionComponent->SetCanEverAffectNavigation(false);
	CollisionComponent->SetGenerateOverlapEvents(true);
	CollisionComponent->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	
	// Bind the Hit event
	CollisionComponent->OnComponentHit.AddDynamic(this, &AExecution_Projectile::OnProjectileHit);
	CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &AExecution_Projectile::OnProjectileOverlap);
	
	RootComponent = CollisionComponent;

	// Create Movement Component
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->ProjectileGravityScale = GravityScale;
	
	// NETWORKING: Ensure movement is synchronized
	SetReplicateMovement(true);
}

void AExecution_Projectile::ResetForReuse()
{
	bHasExploded = false;
}

void AExecution_Projectile::BeginPlay()
{
	// Update speed from config before Super::BeginPlay might run logic (though Super currently just calls BP)
	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		ProjectileMovement->HomingTargetComponent = nullptr;
	}

	Super::BeginPlay();
	
	// Safety: Destroy after 10 seconds if nothing is hit to prevent leaks
	SetLifeSpan(10.0f);
	
	if (ItemContext.Instigator)
	{
		CollisionComponent->MoveIgnoreActors.Add(ItemContext.Instigator);
		
		if (APawn* InstigatorPawn = Cast<APawn>(ItemContext.Instigator))
		{
			CollisionComponent->MoveIgnoreActors.Add(InstigatorPawn);
		}
        
		UE_LOG(LogTemp, Log, TEXT("DEBUG: Projectile Spawned. Instigator is %s"), *ItemContext.Instigator->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("DEBUG: Projectile Spawned but ItemContext.Instigator is NULL!"));
	}

	// Targeting Logic
	if (bIsHoming && TargetingInstance)
	{
		// Ask the strategy for a target based on the Context and current location
		AActor* FoundTarget = TargetingInstance->FindTarget(ItemContext, GetActorLocation());

		if (FoundTarget)
		{
			// Apply Homing settings
			ProjectileMovement->bIsHomingProjectile = true;
			ProjectileMovement->HomingAccelerationMagnitude = HomingAcceleration;
			ProjectileMovement->HomingTargetComponent = FoundTarget->GetRootComponent();
			
			if (bShowDebugVisuals)
			{
				GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Projectile LOCKED ON: %s"), *FoundTarget->GetName()));
			}
		}
		else if (bShowDebugVisuals)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, TEXT("Projectile: No Target Found via Strategy"));
		}
	}
}

void AExecution_Projectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bShowDebugVisuals)
	{
		// Draw a sphere at the projectile location
		DrawDebugSphere(GetWorld(), GetActorLocation(), 25.0f, 12, FColor::Yellow, false, -1.0f, 0, 2.0f);

		// If homing, draw a line to the target
		if (ProjectileMovement->bIsHomingProjectile && ProjectileMovement->HomingTargetComponent.IsValid())
		{
			AActor* TargetActor = ProjectileMovement->HomingTargetComponent->GetOwner();
			if (TargetActor)
			{
				DrawDebugLine(GetWorld(), GetActorLocation(), TargetActor->GetActorLocation(), FColor::Red, false, -1.0f, 0, 2.0f);
			}
		}
	}
}

bool AExecution_Projectile::CanTriggerOnActor(AActor* OtherActor) const
{
	if (!OtherActor || !OtherActor->IsValidLowLevel() || OtherActor == this)
	{
		return false;
	}
	
	// Basic validation to ensure we don't destroy ourselves hitting the instigator instantly
	// Note: Collision Channels are the preferred way to handle this, but code check adds safety.
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

void AExecution_Projectile::TriggerExplosion(AActor* OtherActor, UPrimitiveComponent* OtherComp, const FHitResult* Hit, bool bFromOverlap)
{
	if (bHasExploded || !HasAuthority())
	{
		return;
	}

	bHasExploded = true;
	
	if (bShowDebugVisuals && GEngine)
	{
		const TCHAR* TriggerLabel = bFromOverlap ? TEXT("OVERLAP") : TEXT("HIT");
		const FString ComponentName = OtherComp ? OtherComp->GetName() : TEXT("None");
		FString HitMsg = FString::Printf(TEXT("Projectile %s: %s (Component: %s)"), TriggerLabel, *OtherActor->GetName(), *ComponentName);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, HitMsg);
		UE_LOG(LogTemp, Warning, TEXT("%s"), *HitMsg);
		
		// Draw a point at impact/overlap location for 2 seconds
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

	// Payload Execution
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

	// Visuals/Sound (SpawnSound is handled in Base BeginPlay, here we could add Impact VFX)
	
	// Cleanup
	FinishExecution();
}

void AExecution_Projectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!CanTriggerOnActor(OtherActor))
	{
		return;
	}

	TriggerExplosion(OtherActor, OtherComp, &Hit, false);
}

void AExecution_Projectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!CanTriggerOnActor(OtherActor))
	{
		return;
	}

	TriggerExplosion(OtherActor, OtherComp, &SweepResult, true);
}
