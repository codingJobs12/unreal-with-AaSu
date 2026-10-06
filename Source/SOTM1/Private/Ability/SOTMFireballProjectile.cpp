#include "Ability/SOTMFireballProjectile.h"

#include "AI/SOTMBossVitalComponent.h"
#include "AI/SOTMIsabelAIController.h"
#include "AI/SOTMIsabelBossAIController.h"
#include "Ability/SOTMLightningStunComponent.h"
#include "Components/SphereComponent.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"

ASOTMFireballProjectile::ASOTMFireballProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(25.0f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->OnComponentHit.AddDynamic(this, &ASOTMFireballProjectile::HandleHit);
	SetRootComponent(Collision);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = Speed;
	Movement->MaxSpeed = Speed;
	Movement->ProjectileGravityScale = 0.0f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bShouldBounce = false;

	FireEffect = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("FireEffect"));
	FireEffect->SetupAttachment(Collision);
	FireEffect->bAutoActivate = true;

	static ConstructorHelpers::FObjectFinder<UParticleSystem> FireFinder(
		TEXT("/Game/SuperPowers/Powers/Flame/Particles/P_ContinueFire.P_ContinueFire"));
	if (FireFinder.Succeeded())
	{
		FireParticle = FireFinder.Object;
		FireEffect->SetTemplate(FireParticle);
	}

	SetReplicates(false);
}

void ASOTMFireballProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (FireParticle && FireEffect && FireEffect->Template != FireParticle)
	{
		FireEffect->SetTemplate(FireParticle);
	}

	// Never collide with whoever fired it.
	if (AActor* OwnerActor = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor, true);
	}
	if (APawn* InstigatorPawn = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(InstigatorPawn, true);
	}

	// Destroyed after LifeSeconds if it never hits anything.
	SetLifeSpan(LifeSeconds);

	LaunchTowardAim();
}

void ASOTMFireballProjectile::LaunchTowardAim()
{
	UWorld* World = GetWorld();
	APlayerController* PC = nullptr;
	if (const APawn* InstigatorPawn = GetInstigator())
	{
		PC = Cast<APlayerController>(InstigatorPawn->GetController());
	}
	if (!PC && World)
	{
		PC = World->GetFirstPlayerController();
	}

	FVector Direction = GetActorForwardVector();
	if (PC && World)
	{
		FVector CamLoc;
		FRotator CamRot;
		PC->GetPlayerViewPoint(CamLoc, CamRot);
		FVector CamForward = CamRot.Vector();

		// Exact screen centre (crosshair) when a viewport is available.
		int32 ViewX = 0, ViewY = 0;
		PC->GetViewportSize(ViewX, ViewY);
		FVector CenterOrigin, CenterDir;
		if (ViewX > 0 && ViewY > 0 &&
			PC->DeprojectScreenPositionToWorld(ViewX * 0.5f, ViewY * 0.5f, CenterOrigin, CenterDir))
		{
			CamLoc = CenterOrigin;
			CamForward = CenterDir;
		}

		// Start the trace level with the player so a third-person camera boom doesn't
		// catch geometry behind the character.
		const AActor* Shooter = GetOwner() ? GetOwner() : Cast<AActor>(PC->GetPawn());
		const float AlongToShooter = Shooter
			? FMath::Max(0.0f, FVector::DotProduct(Shooter->GetActorLocation() - CamLoc, CamForward))
			: 0.0f;
		const FVector Start = CamLoc + CamForward * AlongToShooter;
		const FVector End = CamLoc + CamForward * AimTraceDistance;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(FireballAimTrace), false, this);
		if (Shooter)
		{
			Params.AddIgnoredActor(Shooter);
		}
		FHitResult Hit;
		const FVector Target = World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
			? FVector(Hit.ImpactPoint) : End;

		FVector ToTarget = Target - GetActorLocation();
		Direction = ToTarget.SizeSquared() > FMath::Square(100.0f) ? ToTarget.GetSafeNormal() : CamForward;
	}

	LaunchDirection = Direction.GetSafeNormal();
	ApplyLaunch();
	// A Blueprint child's own BeginPlay / movement setup can run after this - re-assert once next frame.
	if (World)
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ASOTMFireballProjectile::ApplyLaunch);
	}
}

void ASOTMFireballProjectile::ApplyLaunch()
{
	if (bConsumed || LaunchDirection.IsNearlyZero())
	{
		return;
	}

	// If the Blueprint (e.g. a reparented BP_Fireball) carries its own Projectile Movement
	// component, it would fight this one and send the ball the wrong way. Keep only ours.
	TInlineComponentArray<UProjectileMovementComponent*> Movements(this);
	for (UProjectileMovementComponent* Other : Movements)
	{
		if (Other && Other != Movement)
		{
			Other->Deactivate();
			Other->DestroyComponent();
		}
	}

	SetActorRotation(LaunchDirection.Rotation());
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->MaxSpeed = Speed;
	Movement->Velocity = LaunchDirection * Speed;
	Movement->UpdateComponentVelocity();
	UE_LOG(LogTemp, Verbose, TEXT("Fireball launch dir=%s"), *LaunchDirection.ToString());
}

bool ASOTMFireballProjectile::IsIsabella(const AActor* Actor)
{
	if (!Actor)
	{
		return false;
	}
	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		const AController* Controller = Pawn->GetController();
		if (Cast<ASOTMIsabelAIController>(Controller) || Cast<ASOTMIsabelBossAIController>(Controller))
		{
			return true;
		}
	}
	if (Actor->FindComponentByClass<USOTMBossVitalComponent>() ||
		Actor->FindComponentByClass<USOTMLightningStunComponent>())
	{
		return true;
	}
	// Last resort for a pure-Blueprint Isabella (e.g. BP_Isabel): match by class name.
	return Actor->GetClass()->GetName().Contains(TEXT("Isabel"), ESearchCase::IgnoreCase);
}

void ASOTMFireballProjectile::HandleHit(
	UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	const FVector NormalImpulse, const FHitResult& Hit)
{
	(void)HitComponent;
	(void)OtherComp;
	(void)NormalImpulse;

	if (bConsumed)
	{
		return;
	}
	if (OtherActor && (OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator()))
	{
		return;
	}
	bConsumed = true;

	const bool bIsabella = IsIsabella(OtherActor);
	if (bIsabella && Damage > 0.0f)
	{
		AController* InstigatorController = GetInstigator() ? GetInstigator()->GetController() : nullptr;
		UGameplayStatics::ApplyDamage(OtherActor, Damage, InstigatorController, this, UDamageType::StaticClass());
	}

	OnFireballHit(OtherActor, bIsabella, Hit.ImpactPoint);
	Destroy();
}
