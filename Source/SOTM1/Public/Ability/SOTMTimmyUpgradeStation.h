#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOTMTimmyUpgradeStation.generated.h"

class USphereComponent;
class UTextRenderComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FSOTMStationPlayerEvent, AActor*);

/**
 * Blueprint-placeable overlap trigger for Timmy's Upgrade Station. Drag an instance of
 * a Blueprint subclass of this (e.g. BP_TimmyUpgradeStation) directly into the level at
 * the station's location, instead of relying on C++ to find the nearest Timmy mesh and
 * spawn one at runtime.
 *
 * This actor's only job is detecting "is a Pawn inside my interaction sphere or not"
 * and broadcasting that as OnPlayerEntered / OnPlayerExited. It knows nothing about
 * the player state, the HUD, or upgrades - all of that lives in
 * USOTMDemoPhase3WorldSubsystem, which finds this placed actor in the level
 * (BindToPlacedTimmyStation) and is the only thing that binds to these events.
 *
 * IMPORTANT: because this is now placed directly in the level, its BeginPlay() runs as
 * part of normal level startup - well before USOTMDemoPhase3WorldSubsystem gets around to
 * finding and binding to it (it does so ~0.9s later, via a timer). If the player happens
 * to already be standing inside the interaction radius before that binding happens, the
 * normal begin-overlap event has already fired with nobody listening. The world subsystem
 * calls NotifyBoundListenersOfExistingOverlaps() right after binding to cover that case.
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMTimmyUpgradeStation : public AActor
{
	GENERATED_BODY()

public:
	ASOTMTimmyUpgradeStation();
	virtual void BeginPlay() override;

	FSOTMStationPlayerEvent OnPlayerEntered;
	FSOTMStationPlayerEvent OnPlayerExited;

	// Call once, right after binding OnPlayerEntered/OnPlayerExited, to catch a player
	// who was already standing inside the interaction radius before the bind happened
	// (see the class comment above for why that can occur for a level-placed actor).
	void NotifyBoundListenersOfExistingOverlaps();

private:
	// Simple, explicit filter: only BP_MenuSystemCharacter0 (the player pawn) triggers
	// OnPlayerEntered/OnPlayerExited. Matches by class name so any other Pawn (enemies,
	// physics props, etc.) that wanders into the sphere is ignored.
	bool IsTargetPlayerActor(const AActor* Actor) const;

	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UTextRenderComponent> StationTitle;
};
