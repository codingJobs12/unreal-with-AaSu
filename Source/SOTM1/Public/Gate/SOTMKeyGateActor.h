#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOTMKeyGateActor.generated.h"

class UBoxComponent;
class UEnhancedInputComponent;
class UInputAction;
class UPointLightComponent;
class UStaticMeshComponent;
class USOTMObjectiveSubsystem;
class USOTMPlayerStateSubsystem;

/** Same signature as USOTMDemoPhase4WorldSubsystem::OnPromptChanged so the HUD can
 * bind a single handler shape to either source. Broadcasts (bVisible, PromptText). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSOTMGatePromptChangedSignature,
	bool, bVisible,
	FText, PromptText);

/**
 * Standalone, self-contained key-locked gate. Separate from the Phase 4 demo
 * chest/key/gate system - this is the Mansion Gate that leads to the Isabel
 * encounter. Overlapping it marks the "Reach the Gate" objective complete and
 * activates "Unlock the Gate with Key"; pressing Interact while carrying the
 * key unlocks it and activates "Defeat Isabel". No visual open animation yet
 * (StatusLight is the only state feedback for now: red while locked, green
 * once unlocked). No WBP is required - place this Actor in the level and
 * assign GateMesh's static mesh in the Details panel.
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMKeyGateActor : public AActor
{
	GENERATED_BODY()

public:
	ASOTMKeyGateActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gate")
	TObjectPtr<USceneComponent> GateRoot;

	/** Assign the gate's static mesh here in the Details panel (no path is hardcoded). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gate")
	TObjectPtr<UStaticMeshComponent> GateMesh;

	/** Box collision the player must overlap to be considered "at the gate". */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gate")
	TObjectPtr<UBoxComponent> OverlapBox;

	/** Simple state indicator - red while locked, green once unlocked. Stands in
	 * for a real open animation/material swap for now. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Gate")
	TObjectPtr<UPointLightComponent> StatusLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate")
	FLinearColor LockedLightColor = FLinearColor(1.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate")
	FLinearColor UnlockedLightColor = FLinearColor(0.0f, 1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate")
	float StatusLightIntensity = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate")
	FText LockedNoKeyPrompt = FText::FromString(TEXT("Locked - find the key"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate")
	FText LockedHasKeyPrompt = FText::FromString(TEXT("Press E to unlock the gate"));

	UPROPERTY(BlueprintAssignable, Category="Gate")
	FSOTMGatePromptChangedSignature OnGatePromptChanged;

	UFUNCTION(BlueprintPure, Category="Gate")
	bool IsUnlocked() const { return bUnlocked; }

	UFUNCTION(BlueprintPure, Category="Gate")
	bool IsPlayerInRange() const { return bPlayerInRange; }

private:
	UFUNCTION()
	void HandleOverlapBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOverlapEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	void BindInteractInput();
	void UnbindInteractInput();
	void HandleInteractInput();

	UFUNCTION()
	void HandlePlayerStateGateProgressChanged(bool bReached, bool bHasKey, bool bUnlockedParam);

	UFUNCTION()
	void HandlePhase4KeyProgressChanged(bool bChestOpened, bool bHasGateKey, bool bGateUnlocked, bool bDemoCompleted);
	void RefreshPrompt();
	void RefreshStatusLight();

	TWeakObjectPtr<USOTMPlayerStateSubsystem> PlayerState;
	TWeakObjectPtr<USOTMObjectiveSubsystem> Objectives;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractInputAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundEnhancedInput;
	uint32 InteractBindingHandle = 0;

	bool bPlayerInRange = false;
	bool bUnlocked = false;

	FTimerHandle BindInputRetryTimer;
};
