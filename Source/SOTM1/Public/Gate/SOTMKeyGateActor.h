#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "SOTMKeyGateActor.generated.h"

class APawn;
class UBoxComponent;
class UEnhancedInputComponent;
class UInputAction;
class UPointLightComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class USOTMObjectiveSubsystem;
class USOTMPlayerStateSubsystem;

/** Same signature as USOTMDemoPhase4WorldSubsystem::OnPromptChanged so the HUD can
 * bind a single handler shape to either source. Broadcasts (bVisible, PromptText). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSOTMGatePromptChangedSignature,
	bool, bVisible,
	FText, PromptText);

/** Broadcast once, right after SpawnIsabelBossIfNeeded() successfully spawns her. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSOTMIsabelBossSpawnedSignature,
	APawn*, SpawnedBoss);

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
class USkeletalMesh;
class USoundBase;
class UCameraComponent;
class ACameraActor;
class UAnimSequence;
class ASkeletalMeshActor;
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

	UPROPERTY(BlueprintAssignable, Category="Isabel Boss")
	FSOTMIsabelBossSpawnedSignature OnIsabelBossSpawned;

	/** Called after Isabella's intro dialogue: moves the player to the InsideArrow component and spawns Isabel (gate stays closed). */
	UFUNCTION(BlueprintCallable, Category="Gate")
	void OpenGateAndSpawnIsabel();

	/**
	 * Isabella intro entrance: the gate swings open degree by degree (0, -1 ... -110), the player WALKS
	 * to the InsideArrow, the gate swings shut again, then OnComplete runs (the dialogue starts there).
	 * Isabella is already standing in the arena from the start (spawned on the floor, AI paused).
	 */
	void PlayIntroEntrance(TFunction<void()> OnComplete);

	/** Component of the gate Blueprint that swings (a scene/mesh component name). Default: the gate mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	FName GateDoorComponentName = TEXT("GateMesh");

	/** Relative yaw of the gate mesh when CLOSED (it swings 0, -1, -2 ... to GateOpenYaw, and back). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	float GateClosedYaw = 0.0f;

	/** Relative yaw of the gate mesh when OPEN. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	float GateOpenYaw = -110.0f;

	/** Degrees per second of the swing (it changes one whole degree at a time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="5.0"))
	float GateDegreesPerSecond = 45.0f;

	/** Walking speed of the player to the InsideArrow (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="50.0"))
	float PlayerWalkSpeed = 220.0f;

	/** Cine camera used ONLY for the entrance (gate opens / player walks in / gate closes). Leave empty to auto-find 'CineCameraActor2'.
	 *  After the gate closes the camera is left exactly as it is - code never moves it or switches away. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Gate|Entrance")
	TObjectPtr<AActor> EntranceCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="0.0"))
	float EntranceCameraBlendTime = 1.0f;

	/** Where Isabella appears. Leave empty to auto-find the level actor 'PointLight2'. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Gate|Entrance")
	TObjectPtr<AActor> IsabelSpawnPoint;

	/** Mesh forced onto Isabella (SK_CruelDoll). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	TSoftObjectPtr<USkeletalMesh> IsabelMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/AI/CruelDoll/Meshes/SK_CruelDoll.SK_CruelDoll")));

	/** Gate creak played when it opens and again when it closes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	TSoftObjectPtr<USoundBase> GateSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/SFX/Temporary/SFX_TEMP_GateOpen.SFX_TEMP_GateOpen")));

	/** Seconds the gate takes to swing fully open / closed (eased in and out). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="0.5"))
	float GateOpenSeconds = 3.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="0.5"))
	float GateCloseSeconds = 2.8f;

	/** Idle animation looped on the cinematic doll. */
	/** Character BP used for Isabella during the intro dialogue (has its own cine camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	TSoftClassPtr<AActor> DemoIsabellaClass = TSoftClassPtr<AActor>(FSoftObjectPath(TEXT("/Game/Sequence/BP_DemoIsabella.BP_DemoIsabella_C")));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	TSoftObjectPtr<UAnimSequence> DollIdleAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/AI/CruelDoll/Animations/AS_CruelDoll_Idle.AS_CruelDoll_Idle")));

	/** Yaw added to the doll so she faces the player (SK_CruelDoll forward axis fix; try 0 or -90). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	float DollYawOffset = 0.0f;

	/** Extra height (cm) added to the doll's feet if she sinks into / floats above the floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	float DollZOffset = 0.0f;

	/** Seconds the player's camera takes to swing to Isabella when her dialogue starts. */
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="0.3"))
	float CameraTurnSeconds = 2.2f;

	/** Slow push-in speed (cm/s) toward Isabella while she speaks. 0 = camera stays still. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="0.0"))
	float CameraPushInSpeed = 12.0f;

	/** Seconds the white flash takes to fade when the dialogue ends and the fight starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance", meta=(ClampMin="0.1"))
	float FlashSeconds = 0.9f;

	/** Black cinematic bars during the intro. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate|Entrance")
	bool bLetterbox = false; // bars now come from USOTMCinematicBarsSubsystem (all sequences)

	/** Ends the intro cinematic visuals (bars, camera drift). Safe to call any time. */
	void EndIntroCinematic();

	UFUNCTION(BlueprintPure, Category="Gate")
	bool IsUnlocked() const { return bUnlocked; }

	UFUNCTION(BlueprintPure, Category="Gate")
	bool IsPlayerInRange() const { return bPlayerInRange; }

	/** Boss pawn class to spawn once the gate is unlocked (assign BP_Isabel here). */
	UPROPERTY(EditAnywhere, Category="Isabel Boss")
	TSubclassOf<APawn> IsabelBossClass;

	/** Placed BP_IsabelArena instance in this level - its first UBoxComponent
	 * (named "Box" in the Blueprint) is used as the boss spawn location. Drag
	 * the level's Arena actor into this slot in the gate's Details panel. */
	UPROPERTY(EditInstanceOnly, Category="Isabel Boss")
	TObjectPtr<AActor> IsabelArenaActor;

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

#if !UE_BUILD_SHIPPING
	// Testing only - F9/F10 keys deal flat debug damage to Isabel, so her health
	// can be tested without building the real attack-hit logic yet. Routes
	// through the standard UGameplayStatics::ApplyDamage flow, same as any real
	// attack would, so it lands on her existing USOTMBossVitalComponent's
	// Current Health exactly like a real hit - no separate health variable is
	// used or needed. FindIsabelBossActor() does NOT rely solely on this gate's
	// own SpawnedIsabelBoss reference (which is null if she was placed in the
	// level manually, or reached via a fast-travel shortcut that skipped the
	// real spawn-on-unlock flow) - it falls back to searching the whole world
	// for any actor carrying a USOTMBossVitalComponent, so the debug keys work
	// regardless of how she ended up in the level.
	void HandleDebugDamage20();
	void HandleDebugDamage50();
	void ApplyDebugDamageToIsabel(float Damage);
	AActor* FindIsabelBossActor() const;
#endif

	UFUNCTION()
	void HandlePlayerStateGateProgressChanged(bool bReached, bool bHasKey, bool bUnlockedParam);

	UFUNCTION()
	void HandlePhase4KeyProgressChanged(bool bChestOpened, bool bHasGateKey, bool bGateUnlocked, bool bDemoCompleted);
	void RefreshPrompt();
	void RefreshStatusLight();

	/** The WidgetComponent marker inside BP_Gate: on while the "reach the gate" mission is active,
	 *  off once the player has reached the gate. */
	void FindGateMarker();
	void RefreshGateMarker();

	UFUNCTION()
	void HandleObjectiveChangedForMarker(FSOTMObjectiveData Objective);

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> GateMarker;

	bool bMarkerDismissed = false;

	/** When false (default) the status light is left exactly as authored: no colour change, no flicker. */
	UPROPERTY(EditAnywhere, Category="Gate|Light")
	bool bToggleStatusLight = false;
	void SpawnIsabelBossIfNeeded();
	void SetIsabelAIPaused(bool bPaused);
	void EntranceTick();
	void FinishEntrance();
	void BeginIsabelReveal(TFunction<void()> OnComplete);
	void SpawnCinematicDoll();
	void AnimateLetterbox(float Target, float Seconds);
	void LetterboxTick();
	void ShowFlash();
	void FlashTick();
	void HideFlash();
	TSharedPtr<class SWidget> FlashWidget;
	TSharedPtr<float> FlashAmount;
	FTimerHandle FlashTimer;
	float FlashElapsed = 0.0f;
	void PlayGateSound();
	/** Real elapsed game seconds since the previous call for this clock (frame-rate independent), clamped to 0.1. */
	float ConsumeDelta(double& LastTime) const;
	double EntranceLastTime = -1.0;
	double LetterboxLastTime = -1.0;
	double FlashLastTime = -1.0;
	double ReadyLastTime = -1.0;
	float DoorSwingFrom = 0.0f;
	float DoorSwingDuration = 3.0f;
	TWeakObjectPtr<UCameraComponent> ShotCameraComp;
	FVector ShotCamBaseLoc = FVector::ZeroVector;
	FRotator ShotCamBaseRot = FRotator::ZeroRotator;
	float ShotCamBaseFov = 90.0f;
	void RevealTick();
	AActor* ResolveEntranceCamera();
	void ShowLetterbox();
	void HideLetterbox();

	FTimerHandle RevealTimer;
	TWeakObjectPtr<AActor> RevealCamera;
	FRotator RevealStartRot = FRotator::ZeroRotator;
	FVector RevealFocus = FVector::ZeroVector;
	float RevealTime = 0.0f;
	bool bRevealTurnDone = false;
	TFunction<void()> RevealComplete;
	TSharedPtr<class SWidget> LetterboxWidget;
	TSharedPtr<float> LetterboxAmount;
	float LetterboxFrom = 0.0f;
	float LetterboxTo = 0.0f;
	float LetterboxElapsed = 0.0f;
	float LetterboxDuration = 1.0f;
	FTimerHandle LetterboxTimer;
	FRotator SavedControlRot = FRotator::ZeroRotator;
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> ShotCamera;
	TWeakObjectPtr<AActor> ShotRestoreTarget;
	bool bHasSavedControlRot = false;
	FVector DollFloorLocation = FVector::ZeroVector;
	UPROPERTY(Transient)
	TObjectPtr<AActor> CinematicDoll;

	enum class EEntrancePhase : uint8 { None, Opening, Walking, Closing };
	EEntrancePhase EntrancePhase = EEntrancePhase::None;
	TFunction<void()> EntranceComplete;
	FTimerHandle EntranceTimer;
	FTimerHandle IsabelReadyTimer;
	float IsabelReadyTimeLeft = 0.0f;
	float IsabelReadyStableTime = 0.0f;
	TWeakObjectPtr<USceneComponent> DoorComponent;
	TWeakObjectPtr<APawn> WalkingPawn;
	float DoorYawCurrent = 0.0f;
	float DoorYawClosed = 0.0f;
	float DoorYawAccumulator = 0.0f;
	float WalkTimeLeft = 0.0f;
	FVector WalkTarget = FVector::ZeroVector;

	TWeakObjectPtr<USOTMPlayerStateSubsystem> PlayerState;
	TWeakObjectPtr<USOTMObjectiveSubsystem> Objectives;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractInputAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundEnhancedInput;
	uint32 InteractBindingHandle = 0;

	bool bPlayerInRange = false;
	bool bUnlocked = false;

	UPROPERTY(Transient)
	TObjectPtr<APawn> SpawnedIsabelBoss;

	FTimerHandle BindInputRetryTimer;
};
