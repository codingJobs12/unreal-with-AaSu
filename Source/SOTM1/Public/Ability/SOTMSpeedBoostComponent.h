#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOTMSpeedBoostComponent.generated.h"

class APawn;
class UCharacterMovementComponent;
class USkeletalMeshComponent;
class FBoolProperty;

/**
 * Speed Boost (Q) movement effect - works the same way as the Shift test boost: it sets
 * CharacterMovementComponent::MaxWalkSpeed directly. While boosting, MaxWalkSpeed is held at
 * NormalWalkSpeed * Multiplier; when the boost ends it goes straight back to NormalWalkSpeed.
 *
 * NormalWalkSpeed is the character Blueprint's default walk speed (read from the movement
 * component's archetype), so the boost is always 2x *normal* speed.
 *
 * The value is re-applied every frame after the pawn's Blueprint Tick and right before
 * CharacterMovement reads it, so no Blueprint logic can overwrite it mid-boost:
 *     PlayerController (input) -> Pawn Tick (Blueprint) -> THIS -> CharacterMovement
 * Animation: the character's locomotion blend space tops out at normal speed, so at 2x speed
 * the feet would slide. While boosting, the mesh's GlobalAnimRateScale is scaled by
 * (current ground speed / normal walk speed), so the walk/run cycle plays up to 2x faster
 * exactly in step with the extra speed, stays normal when standing still or in the air, and
 * is restored when the boost ends.
 *
 * VFX: this does NOT manage the "P_Rays" particle effects itself - it only sets the
 * Blueprint's own Sprint state variable, "iSSprinting" (by reflection - no Blueprint edit),
 * true while boosting and false the instant the boost ends. BP_MenuSystemCharacter's own
 * Sprint logic already Activates/positions/Deactivates those effects correctly every Tick
 * whenever iSSprinting is true, with none of Shift's cost or lingering-particle problems -
 * an earlier version of this component ALSO drove those particle components directly
 * (Activate/Deactivate/KillParticlesForced) on top of setting iSSprinting, which fought the
 * Blueprint's own management: it caused a particle to be re-triggered and left hanging after
 * the boost, and doubled per-frame VFX work, which is what caused the FPS drop during Speed
 * Boost that Shift never had. Only iSSprinting is touched now, so the VFX behaves exactly as
 * it does for Shift because it IS Shift's own logic running, just triggered from C++ instead
 * of a held key. BP_MenuSystemCharacter itself is never edited.
 *
 * iSSprinting is only held true while the pawn is actually moving on the ground above a
 * small speed threshold - not for the whole boost duration - so the VFX switches on and
 * off with the player's actual movement (no effect while standing still), the same as it
 * would look if the player tapped Shift on and off in step with moving and stopping.
 */
UCLASS(ClassGroup=(SOTM))
class SOTM1_API USOTMSpeedBoostComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOTMSpeedBoostComponent();

	/** Returns the pawn's boost component, creating and registering one if needed. */
	static USOTMSpeedBoostComponent* FindOrAddTo(APawn* Pawn);

	/** Starts boosting at NormalWalkSpeed * InMultiplier. False if the pawn cannot move. */
	bool StartBoost(float InMultiplier);

	/** Ends the boost and puts MaxWalkSpeed back to NormalWalkSpeed. */
	void StopBoost();

	bool IsBoosting() const { return bBoosting; }
	float GetNormalWalkSpeed() const { return NormalWalkSpeed; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool EnsureInitialized();
	void SetSprintFlag(bool bValue) const;
	/** Turns the VFX on/off to match whether the pawn is actually moving right now. */
	void UpdateVfxFromSpeed() const;

	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;

	bool bInitialized = false;
	bool bBoosting = false;
	float NormalWalkSpeed = 0.0f;
	float BoostedWalkSpeed = 0.0f;
	float NormalAcceleration = 0.0f;
	float BoostMultiplier = 1.0f;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Mesh;

	/** Mesh GlobalAnimRateScale before the boost, restored afterwards. */
	float NormalAnimRate = 1.0f;

	/** Cached once in EnsureInitialized so the boost doesn't do a property-name lookup every frame. */
	FBoolProperty* SprintFlagProperty = nullptr;

	// Diagnostics for Saved/Logs.
	float PeakGroundSpeed = 0.0f;
	float SecondsSinceLog = 0.0f;
};
