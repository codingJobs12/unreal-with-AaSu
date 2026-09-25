#include "Ability/SOTMSpeedBoostComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMSpeedBoost, Log, All);

namespace SOTMSpeedBoostPrivate
{
	// The Blueprint's own Sprint state variable - see the class comment for what it drives.
	const TCHAR* SprintFlagPropertyName = TEXT("iSSprinting");

	// Below this ground speed (cm/s) the pawn counts as "standing still" for VFX purposes.
	constexpr float MovingSpeedThreshold = 10.0f;
}

USOTMSpeedBoostComponent::USOTMSpeedBoostComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // only ticks while boosting
	PrimaryComponentTick.TickGroup = TG_PrePhysics;     // ordered against movement by prerequisites
}

USOTMSpeedBoostComponent* USOTMSpeedBoostComponent::FindOrAddTo(APawn* Pawn)
{
	if (!Pawn)
	{
		return nullptr;
	}
	if (USOTMSpeedBoostComponent* Existing = Pawn->FindComponentByClass<USOTMSpeedBoostComponent>())
	{
		return Existing;
	}
	USOTMSpeedBoostComponent* Created = NewObject<USOTMSpeedBoostComponent>(
		Pawn, USOTMSpeedBoostComponent::StaticClass(), TEXT("SOTM_SpeedBoost"), RF_Transient);
	Pawn->AddInstanceComponent(Created);
	Created->RegisterComponent();
	return Created;
}

void USOTMSpeedBoostComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();
}

bool USOTMSpeedBoostComponent::EnsureInitialized()
{
	if (bInitialized && Movement)
	{
		return true;
	}

	const ACharacter* Character = Cast<ACharacter>(GetOwner());
	Movement = Character ? Character->GetCharacterMovement() : nullptr;
	Mesh = Character ? Character->GetMesh() : nullptr;

	if (AActor* Owner = GetOwner())
	{
		// Cached once here, not looked up every frame - see SprintFlagProperty's comment.
		SprintFlagProperty = FindFProperty<FBoolProperty>(Owner->GetClass(), SOTMSpeedBoostPrivate::SprintFlagPropertyName);
		if (!SprintFlagProperty)
		{
			UE_LOG(LogSOTMSpeedBoost, Warning,
				TEXT("Could not find bool property '%s' on %s - Speed Boost will not trigger the P_Rays VFX."),
				SOTMSpeedBoostPrivate::SprintFlagPropertyName, *GetNameSafe(Owner));
		}
	}

	if (!Movement)
	{
		UE_LOG(LogSOTMSpeedBoost, Warning, TEXT("No CharacterMovementComponent on %s - Speed Boost cannot run."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	// Normal speed = the Blueprint's default walk speed, not whatever MaxWalkSpeed happens to
	// be right now (it could be a temporary sprint value).
	const UCharacterMovementComponent* Defaults = Cast<UCharacterMovementComponent>(Movement->GetArchetype());
	NormalWalkSpeed = FMath::Max(1.0f, Defaults ? Defaults->MaxWalkSpeed : Movement->MaxWalkSpeed);
	NormalAcceleration = Defaults ? Defaults->MaxAcceleration : Movement->MaxAcceleration;

	// Run after the pawn's own Tick (the pawn already ticks after its PlayerController, where
	// input events fire)...
	AddTickPrerequisiteActor(GetOwner());
	// ...and before CharacterMovement uses MaxWalkSpeed this frame.
	Movement->PrimaryComponentTick.AddPrerequisite(this, PrimaryComponentTick);

	bInitialized = true;
	return true;
}

void USOTMSpeedBoostComponent::SetSprintFlag(const bool bValue) const
{
	if (SprintFlagProperty)
	{
		SprintFlagProperty->SetPropertyValue_InContainer(GetOwner(), bValue);
	}
}

void USOTMSpeedBoostComponent::UpdateVfxFromSpeed() const
{
	const bool bShouldShowVfx = bBoosting && Movement
		&& Movement->IsMovingOnGround()
		&& Movement->Velocity.Size2D() > SOTMSpeedBoostPrivate::MovingSpeedThreshold;
	SetSprintFlag(bShouldShowVfx);
}

bool USOTMSpeedBoostComponent::StartBoost(const float InMultiplier)
{
	if (!EnsureInitialized())
	{
		return false;
	}

	BoostMultiplier = FMath::Max(1.0f, InMultiplier);
	BoostedWalkSpeed = NormalWalkSpeed * BoostMultiplier;
	NormalAnimRate = Mesh ? Mesh->GlobalAnimRateScale : 1.0f;

	Movement->MaxWalkSpeed = BoostedWalkSpeed;
	Movement->MaxAcceleration = NormalAcceleration * FMath::Max(1.0f, InMultiplier); // reach it quickly
	PeakGroundSpeed = 0.0f;
	SecondsSinceLog = 0.0f;
	bBoosting = true;
	SetComponentTickEnabled(true);

	// Let the Blueprint's own Sprint logic drive the P_Rays VFX - see the class comment for
	// why this component does not also touch those particle components directly. Evaluated
	// here too (not just from TickComponent) so the very first frame is already correct.
	UpdateVfxFromSpeed();

	UE_LOG(LogSOTMSpeedBoost, Display, TEXT("START pawn=%s normal=%.1f boosted=%.1f (x%.2f)"),
		*GetNameSafe(GetOwner()), NormalWalkSpeed, BoostedWalkSpeed, InMultiplier);
	return true;
}

void USOTMSpeedBoostComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBoosting || !Movement)
	{
		return;
	}

	// Hold the boosted speed every frame, right before movement reads it.
	Movement->MaxWalkSpeed = BoostedWalkSpeed;

	// Keep the VFX state matched to actual movement every frame (see the class comment) -
	// it may itself get cleared by the Blueprint's own logic when it sees the physical Shift
	// key is not held (which it never is here), so this has to be re-asserted continuously,
	// not just once. This one property write is the ONLY thing driving the P_Rays VFX.
	UpdateVfxFromSpeed();

	// Speed the walk/run cycle up in step with the extra speed: at normal speed or slower the
	// animation plays at its normal rate, at 2x normal speed it plays 2x. Only on the ground -
	// jump/fall animations keep their normal rate. Smoothed so it never pops.
	const float GroundSpeed = static_cast<float>(Movement->Velocity.Size2D());
	if (Mesh)
	{
		const float SpeedRatio = Movement->IsMovingOnGround()
			? FMath::Clamp(GroundSpeed / NormalWalkSpeed, 1.0f, BoostMultiplier)
			: 1.0f;
		Mesh->GlobalAnimRateScale = FMath::FInterpTo(
			Mesh->GlobalAnimRateScale, NormalAnimRate * SpeedRatio, DeltaTime, 12.0f);
	}

	PeakGroundSpeed = FMath::Max(PeakGroundSpeed, GroundSpeed);
	SecondsSinceLog += DeltaTime;
	if (SecondsSinceLog >= 1.0f)
	{
		SecondsSinceLog = 0.0f;
		UE_LOG(LogSOTMSpeedBoost, Display, TEXT("ACTIVE maxWalkSpeed=%.1f groundSpeed=%.1f animRate=%.2f"),
			Movement->MaxWalkSpeed, GroundSpeed, Mesh ? Mesh->GlobalAnimRateScale : 1.0f);
	}
}

void USOTMSpeedBoostComponent::StopBoost()
{
	if (!bBoosting)
	{
		return;
	}
	bBoosting = false;
	SetComponentTickEnabled(false);

	if (Movement)
	{
		Movement->MaxWalkSpeed = NormalWalkSpeed;
		Movement->MaxAcceleration = NormalAcceleration;
	}
	if (Mesh)
	{
		Mesh->GlobalAnimRateScale = NormalAnimRate; // walk animation back to normal speed
	}
	// Same as releasing Shift - the Blueprint's own logic Deactivates and cleans up the
	// P_Rays VFX itself, exactly as it does for a real Shift release. bBoosting is already
	// false at this point, so UpdateVfxFromSpeed() always turns it off here.
	UpdateVfxFromSpeed();

	UE_LOG(LogSOTMSpeedBoost, Display, TEXT("STOP back to normal=%.1f peakGroundSpeedDuringBoost=%.1f"),
		NormalWalkSpeed, PeakGroundSpeed);
}

void USOTMSpeedBoostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopBoost();
	Super::EndPlay(EndPlayReason);
}
