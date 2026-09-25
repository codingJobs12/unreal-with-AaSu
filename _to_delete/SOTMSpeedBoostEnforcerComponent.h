#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOTMSpeedBoostEnforcerComponent.generated.h"

class UCharacterMovementComponent;

/**
 * Runtime-only fix, added dynamically to whatever pawn spawns (BP_MenuSystemCharacter in
 * production) by USOTMPlayerFoundationWorldSubsystem::TryBindPlayer - same "don't touch the
 * Blueprint" pattern already used there for USOTMPlayerVitalComponent and
 * USOTMFlashlightFixComponent.
 *
 * The bug: BP_MenuSystemCharacter has its own built-in Sprint system (IA_Sprint /
 * "iSSprinting") that re-writes CharacterMovementComponent::MaxWalkSpeed every Tick based on
 * whether the player is currently holding Sprint. USOTMDemoPhase3WorldSubsystem::
 * TryActivateSpeedBoost() sets MaxWalkSpeed to a boosted value once, and its presentation
 * timer only re-applies it every 0.1s - so for nearly all of every 0.1s window, the
 * Blueprint's own Tick was silently stomping the boosted value straight back down, which is
 * why the boost was invisible in play even though the HUD correctly showed it as ACTIVE.
 *
 * The fix: every frame, in TG_PostUpdateWork (after the Pawn's own Tick, so after the
 * Blueprint's Sprint logic has already run and possibly stomped MaxWalkSpeed for this frame),
 * ask USOTMDemoPhase3WorldSubsystem whether Speed Boost is currently Active and, if so,
 * force MaxWalkSpeed back to the boosted value right before the frame renders. When Speed
 * Boost is not active this is a no-op, so Sprint and every other speed system behaves exactly
 * as the Blueprint already implements it.
 */
UCLASS(ClassGroup=(SOTM), meta=(BlueprintSpawnableComponent))
class SOTM1_API USOTMSpeedBoostEnforcerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOTMSpeedBoostEnforcerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UCharacterMovementComponent> Movement;
};
