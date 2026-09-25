#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SOTMFlashlightFixComponent.generated.h"

class USpotLightComponent;
class UStaticMeshComponent;

/**
 * Runtime-only fix for the player flashlight, added dynamically to whatever pawn spawns
 * (BP_MenuSystemCharacter in production) by USOTMPlayerFoundationWorldSubsystem::TryBindPlayer
 * - same "don't touch the Blueprint" pattern already used there for USOTMPlayerVitalComponent.
 *
 * Two things fixed, both driven from here:
 *   1. SpotLightComponent::SourceRadius is forced to 200 once at BeginPlay.
 *   2. The flicker: the Blueprint currently repositions its SpotLightComponent every Tick
 *      by reading the hand socket (GetSocketLocation/GetSocketRotation -> SetWorldLocation/
 *      SetWorldRotation). That read can land a frame behind the fully-updated animation
 *      pose, so during fast arm motion (walking) the light visibly clips through/behind the
 *      hand - which reads as flicker. Rather than edit that Blueprint graph, this component
 *      rigidly attaches the SpotLight under the "Flashlight" mesh component once (so it
 *      inherits a correct transform for free), and then - every frame, in TG_PostUpdateWork,
 *      which runs AFTER the Pawn's own Tick and after animation evaluation - hard-snaps the
 *      SpotLight to the flashlight mesh's current, fully up-to-date transform. Whatever the
 *      Blueprint's own Tick script computed earlier in the frame is always overwritten with
 *      the correct position before the frame renders, so the flicker is gone regardless of
 *      whether that old Blueprint logic is ever removed.
 */
UCLASS(ClassGroup=(SOTM), meta=(BlueprintSpawnableComponent))
class SOTM1_API USOTMFlashlightFixComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USOTMFlashlightFixComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void FindTargets();

	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> SpotLight;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FlashlightMesh;
};
