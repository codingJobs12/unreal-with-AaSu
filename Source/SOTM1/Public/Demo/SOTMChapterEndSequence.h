#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "GameFramework/Actor.h"
#include "SOTMChapterEndSequence.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UNiagaraSystem;
class USoundBase;
class ACameraActor;
class SWidget;
class USceneComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSOTMChapterEndFinishedSignature);

/**
 * Chapter 1 ending, driven by one placed actor you configure in the Details panel:
 *   1. Isabella plays her death montage + her LowHealth line, then FREEZES in the last pose.
 *   2. Timmy appears.
 *   3. Timmy's Defeated line plays.
 *   4. The spider spawns.
 *   5. Timmy collapses in fear.
 *   6. Screen fades to black, "END OF CHAPTER 1", then back to the main menu.
 *
 * Trigger it from BP_Isabel (low health or death) with the node "Play Chapter End".
 * It only ever plays once. Dialogue comes from the boss dialogue table
 * (rows LowHealth_Mother and Defeated_Timmy by default).
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMChapterEndSequence : public AActor
{
	GENERATED_BODY()

public:
	ASOTMChapterEndSequence();

	/** Starts the sequence. Pass Isabella (self from BP_Isabel). Safe to call repeatedly. */
	UFUNCTION(BlueprintCallable, Category="Chapter End")
	void PlayChapterEnd(AActor* IsabellaActor);

	/** Finds the placed Chapter End actor in the world and plays it. Use this from BP_Isabel. */
	UFUNCTION(BlueprintCallable, Category="Chapter End", meta=(WorldContext="WorldContextObject"))
	static bool TriggerChapterEnd(UObject* WorldContextObject, AActor* IsabellaActor);

	UFUNCTION(BlueprintPure, Category="Chapter End")
	bool HasPlayed() const { return bStarted; }

	UPROPERTY(BlueprintAssignable, Category="Chapter End")
	FSOTMChapterEndFinishedSignature OnChapterEndFinished;

	// ---- Isabella ----
	/** Her dying animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Isabella")
	TObjectPtr<UAnimMontage> IsabelDeathMontage;

	/** Dialogue table row played while she dies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Isabella")
	FName LowHealthDialogueRow = TEXT("LowHealth_Mother");

	/** Extra seconds added to the death animation (it plays slightly slower so it lasts this much longer). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Isabella")
	float DeathExtraSeconds = 0.4f;

	// ---- Timmy ----
	/** Spawned if TimmyActor is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	TSubclassOf<AActor> TimmyClass;

	/** Optional: a Timmy already placed (hidden) in the level. It is shown instead of spawning. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<AActor> TimmyActor;

	/** Without a spawn point, Timmy appears this far to Isabella's right. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	float TimmyDistanceFromIsabella = 220.0f;

	/** Where Timmy appears (any actor, e.g. a Target Point). Empty = the 'ArrowTimmy' component inside BP_Isabel (or, if missing, Isabella's right side). */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<AActor> TimmySpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<UAnimMontage> TimmyAppearMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<UNiagaraSystem> TimmyAppearEffect;

	/** Animation played straight on Timmy's mesh when he collapses. Empty = /Game/HorrorBear/Animations/Anim_HorrorBear_Death. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<UAnimSequenceBase> TimmyCollapseAnimation;

	/** Walk cycle played (looping) while Timmy walks in. It has no root motion, so Timmy is moved along by code.
	 * Empty = /Game/HorrorBear/Animations/Anim_HorrorBear_Walk. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<UAnimSequenceBase> TimmyWalkAnimation;

	/** Play rate of the walk cycle (raise it if the feet slide, lower it if they run too fast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy", meta=(ClampMin="0.1"))
	float TimmyWalkAnimRate = 1.0f;

	/** Only used when no animation is available (older fallback). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<UAnimMontage> TimmyCollapseMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	FName DefeatedDialogueRow = TEXT("Defeated_Timmy");

	/** Optional: a spider already placed in the level (it is hidden until the slam, then shown at ArrowSpider and dropped). Empty = SpiderClass is spawned. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spider")
	TObjectPtr<AActor> SpiderActor;

	/** If ArrowSpider is on the same side of Isabella as Timmy, mirror it to the opposite side so they never look like a team. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	bool bSpiderOppositeTimmy = true;

	/** Where Timmy starts his walk from (any actor, e.g. a Target Point off-screen). Empty = his pre-placed position, or a spot behind his mark. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Timmy")
	TObjectPtr<AActor> TimmyWalkStartPoint;

	/** Without a start point, Timmy walks in from this far behind his mark (away from Isabella). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy", meta=(ClampMin="0.0"))
	float TimmyWalkInDistance = 500.0f;

	/** Timmy appears AFTER Isabella's death zoom and reaches his spot in this many seconds (0 = use TimmyWalkSpeed instead). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy", meta=(ClampMin="0.0"))
	float TimmyWalkInSeconds = 1.8f;

	/** Timmy's walk speed (cm/s) while he walks in (only used when TimmyWalkInSeconds is 0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy", meta=(ClampMin="50.0"))
	float TimmyWalkSpeed = 220.0f;

	/** True: the collapse plays the montage's animation straight on Timmy's mesh (always visible). False: plays the montage through his Animation Blueprint (needs a matching slot). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	bool bCollapsePlayDirectly = true;

	/** Row played by the spider right after it hits the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	FName SpiderDialogueRow = TEXT("Spider_Landing");

	/** Row Timmy answers with after the spider's line; his collapse montage plays when it ends. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	FName TimmyOutroDialogueRow = TEXT("Timmy_Outro");

	/** If true and no collapse montage is set (or after it ends), Timmy ragdolls to the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timmy")
	bool bRagdollOnCollapse = true;

	// ---- Spider ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	TSubclassOf<AActor> SpiderClass;

	/** Without a spawn point, the spider appears this far behind Isabella (away from the camera). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	float SpiderDistanceBehindIsabella = 500.0f;

	/** Where the spider spawns. Empty = the 'ArrowSpider' component inside BP_Isabel (or, if missing, behind Isabella). */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spider")
	TObjectPtr<AActor> SpiderSpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	TObjectPtr<USoundBase> SpiderSpawnSound;

	/** Spider hangs here first (BP_Arrow): location + rotation. Empty = auto-find the level actor 'BP_Arrow'. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spider")
	TObjectPtr<AActor> SpiderHangStart;

	/** Spider lowers to here (BP_Arrow2): location + rotation, still in the air. Empty = auto-find 'BP_Arrow2'. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spider")
	TObjectPtr<AActor> SpiderHangEnd;

	/** Where the spider slams down (BP_Arrow3): location + rotation. Empty = auto-find the level actor 'BP_Arrow3'. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spider")
	TObjectPtr<AActor> SpiderSlamPoint;

	/** Camera used for the slam (CineCameraActor5). Empty = auto-find 'CineCameraActor5'. It is not moved until the spider has landed. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Spider")
	TObjectPtr<ACameraActor> SpiderSlamCamera;

	/** Extra height (cm) the spider waits at after the cut to CineCameraActor5, just before the slam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	float SpiderSlamStartExtraHeight = 150.0f;

	/** Shake on the slam camera when the spider hits the ground (position in cm, rotation in degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.0"))
	float SlamShakeAmplitude = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.0"))
	float SlamShakeRotation = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.1"))
	float SlamShakeSeconds = 1.1f;

	/** true: the slam camera moves between these exact world Z heights after the slam. false: it rises SlamCameraRise from where it is placed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	bool bSlamCameraAbsoluteZ = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	float SlamCameraStartZ = -650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	float SlamCameraEndZ = -550.0f;

	/** After the slam the slam camera rises this much on the Z axis (cm), smoothly, while the spider speaks (-700 -> -600 = 100). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	float SlamCameraRise = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.2"))
	float SlamCameraRiseSeconds = 4.0f;

	/** Extra 180 degree roll on top of BP_Arrow's rotation. Leave OFF when BP_Arrow already has the upside-down rotation (e.g. Roll=180). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider")
	bool bSpiderHangUpsideDown = false;

	/** The spider is lowered at most this far (cm) from BP_Arrow toward BP_Arrow2 while it hangs (it does not drop all the way). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.0"))
	float SpiderHangDropDistance = 270.0f;

	/** Seconds the spider takes to be lowered from BP_Arrow to BP_Arrow2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.5"))
	float SpiderDescendSeconds = 5.0f;

	/** Pendulum swing while the spider hangs: sideways distance (cm), tilt (degrees) and speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider|Hang")
	float SpiderSwingDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider|Hang")
	float SpiderSwingDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider|Hang")
	float SpiderSwingSpeed = 2.0f;

	/** Slow zoom toward Timmy after the camera has moved to him: distance (cm) and duration (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float TimmyZoomDistance = 30.0f;

	/** How quickly the camera drifts toward Timmy (lower = slower, smoother zoom). */
	UPROPERTY(EditAnywhere, Category="Camera", meta=(ClampMin="0.05"))
	float TimmyCameraMoveSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float TimmyZoomSeconds = 4.0f;

	/** Seconds the spider's camera takes to drop down to ground level before the slam (short = a sudden drop). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.2"))
	float SpiderCameraDownSeconds = 0.25f;

	/** Camera height above the floor (cm) after it moved down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spider", meta=(ClampMin="0.0"))
	float SpiderCameraGroundHeight = 45.0f;

	/** Camera cuts between Isabella / Timmy / Spider cameras use a white flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bFlashOnCameraCut = true;

	// ---- Camera (optional) ----
	/** Optional override. Leave EMPTY to use the CineCamera inside BP_Isabel (default). */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Camera")
	TObjectPtr<ACameraActor> SequenceCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float CameraBlendTime = 1.0f;

	// ---- Timing (seconds) ----
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float DelayBeforeTimmyAppears = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float DelayBeforeTimmyDialogue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float DelayBeforeSpider = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float DelayBeforeCollapse = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float DelayBeforeFade = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float FadeDuration = 2.5f;

	/** How long "END OF CHAPTER 1" stays before returning to the menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing")
	float EndCardDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End")
	FText EndCardText = FText::FromString(TEXT("END OF CHAPTER 1"));

	/** Level opened after the end card. Leave as is for the main menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="End")
	FName NextLevel = TEXT("/Game/Main_Menu_Map");

	// ---- Dynamic camera & dramatic spider entrance ----
	/** Slow push-in, look-at Timmy / spider, and shake on the spider reveal. Turn off for a locked camera. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bDynamicCamera = true;

	/** The camera creeps forward this far (max) over the whole ending. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float CameraDollyDistance = 180.0f;

	/** Creep speed in units per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float CameraDollySpeed = 30.0f;

	/** How strongly the camera turns toward Timmy's spot once Isabella's death animation ends (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0", ClampMax="1.0"))
	float TimmyFocusBlend = 0.9f;

	/** How close (units) the camera stands to Timmy - he is a small character, so keep this low. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="50.0"))
	float TimmyCameraDistance = 260.0f;

	/** The flash and shake play first; the spider appears this many (real) seconds later, after the flash. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float SpiderSpawnDelayAfterFlash = 0.7f;

	/** On the spider reveal the camera snaps back this far (a "recoil"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float SpiderCameraPullBack = 150.0f;

	/** Dutch tilt (degrees) that creeps in on the spider reveal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	float SpiderDutchRoll = 5.0f;

	/** 0 = no shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float SpiderShakeStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.1"))
	float SpiderShakeSeconds = 1.6f;

	/** Brief slow motion at the moment the spider appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bSpiderSlowMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.05", ClampMax="1.0"))
	float SpiderSlowMotionScale = 0.3f;

	/** Real-time seconds the slow motion lasts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.1"))
	float SpiderSlowMotionSeconds = 0.8f;

	/** White flash that hides the pop-in at the moment the spider appears. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	bool bSpiderFlash = true;

	/** The spider spawns where you put it (e.g. in the air) and hangs for this long before it slams down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float SpiderSlamHangTime = 0.25f;

	/** How hard it is pulled down (units/s^2). Higher = a harder, faster slam. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="500.0"))
	float SpiderSlamAcceleration = 9000.0f;

	/** Optional Niagara burst (dust / shockwave) where it hits the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	TObjectPtr<UNiagaraSystem> SpiderLandEffect;

	/** Camera shake (seconds) when the spider lands. 0 = none. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0"))
	float SpiderLandShakeSeconds = 0.9f;

	/** Tilts the camera down by this many degrees for Timmy (rotation only - the camera does not move). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic", meta=(ClampMin="0.0", ClampMax="60.0"))
	float TimmyCameraPitchDown = 15.0f;

	/** Optional Niagara burst (smoke / dark energy) at the spider's spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cinematic")
	TObjectPtr<UNiagaraSystem> SpiderSpawnEffect;

	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Height (pixels) of the black cinematic bars at the top and bottom during the sequence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter End|Cinematic")
	float LetterboxHeight = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter End|Cinematic")
	bool bLetterbox = false; // bars now come from USOTMCinematicBarsSubsystem (all sequences)

private:
	void AnimateBars(float Target, float Seconds);
	void RemoveBars();
	TSharedPtr<class SWidget> BarsRoot;
	TSharedPtr<float> BarsAmount;
	FTSTicker::FDelegateHandle BarsTickerHandle;
public:

private:
	void StepIsabelDies();
	void StepFreezeIsabel();
	void StepTimmyAppears();
	void StepTimmyDialogue();
	void StepTimmyArrived();
	void TryStartTimmyDialogue();
	int32 PlayTimmyAnimation(UAnimSequenceBase* Sequence, bool bLoop, float Rate);
	void RestoreTimmyAnimBlueprint();
	void StepSpiderDialogue();
	void StepSpiderSpawns();
	void StepTimmyCollapses();
	void StepFadeOut();
	void StepEndCard();
	void StepLeave();

	/** Runs Next after the dialogue row finishes (or after a safety timeout). */
	void WhenDialogueFinishes(FName Row, TFunction<void()> Next);
	void After(float Seconds, TFunction<void()> Next);
	void ClearAllTimers();

	FVector ResolvePoint(const AActor* Point, const FVector& Fallback) const;
	void HideHud();
	void LockPlayer();

	enum class ECamPhase : uint8 { Death, Timmy, Spider, Collapse };
	void StartCameraDirector(USceneComponent* Camera);
	void StartSpiderReveal(const FVector& FinalLocation);
	void SpawnSpiderNow();
	void FlashCut(TFunction<void()> AtPeak);
	class UCameraComponent* PickCamera(AActor* Actor) const;
	bool CutToActorCamera(AActor* Actor, bool bDetachCamera);
	void BeginSpiderCameraDown();
	void FlipAndDropSpiderCamera();
	TWeakObjectPtr<AActor> ActiveSlamCam;
	bool bSlamCamRising = false;
	float SlamCamElapsed = 0.0f;
	float SlamCamBaseZ = 0.0f;
	float SlamCamRiseTotal = 100.0f;
	FVector2D SlamZoomDir = FVector2D::ZeroVector;

	/** Small push-in of the slam camera on impact: distance (cm) and duration (s). */
	UPROPERTY(EditAnywhere, Category="Spider|Slam")
	float SlamZoomDistance = 60.0f;

	UPROPERTY(EditAnywhere, Category="Spider|Slam")
	float SlamZoomSeconds = 1.0f;
	FRotator SlamCamBaseRot = FRotator::ZeroRotator;
	FVector2D SlamCamBaseXY = FVector2D::ZeroVector;
	bool bSlamCamShaking = false;
	bool bSlamCamMode = false;
	void RevealSpiderShot();
	bool bSpiderHangPrepared = false;
	void BeginSpiderSlam();
	void UpdateSpiderShot(float RealDelta);

	enum class ESpiderShot : uint8 { None, Descend, Flash, CamDown };
	ESpiderShot SpiderShot = ESpiderShot::None;
	float SpiderShotElapsed = 0.0f;
	FVector SpiderHangStartLoc = FVector::ZeroVector;
	FVector SpiderHangEndLoc = FVector::ZeroVector;
	FVector SpiderHangDropLoc = FVector::ZeroVector;
	FQuat SpiderHangStartQuat = FQuat::Identity;
	FQuat SpiderHangEndQuat = FQuat::Identity;
	TWeakObjectPtr<class UCameraComponent> SpiderShotCam;
	FVector SpiderCamFromLoc = FVector::ZeroVector;
	FVector SpiderCamToLoc = FVector::ZeroVector;
	FVector SpiderCamRestLoc = FVector::ZeroVector;
	FQuat SpiderCamAuthoredQuat = FQuat::Identity;
	bool bSpiderCamTrack = false;
	float SpiderCamShakeLeft = 0.0f;
	void RestoreTimeDilation();

	TWeakObjectPtr<USceneComponent> DirCamera;
	bool bCamDirecting = false;
	ECamPhase CamPhase = ECamPhase::Death;
	FVector CamBaseLoc = FVector::ZeroVector;
	FRotator CamBaseRot = FRotator::ZeroRotator;
	FVector CamCurLoc = FVector::ZeroVector;
	FQuat CamCurRot = FQuat::Identity;
	float CamElapsed = 0.0f;
	float PullBackCur = 0.0f;
	float RollCur = 0.0f;
	float ShakeLeft = 0.0f;
	bool bSlowMoActive = false;
	float SlowMoLeft = 0.0f;
	bool bSpiderSlam = false;
	bool bSpiderDialogueStarted = false;
	float SpiderSlamHangLeft = 0.0f;
	float SpiderSlamVelocity = 0.0f;
	float SpiderSlamTargetZ = 0.0f;
	FVector SpiderImpactPoint = FVector::ZeroVector;
	FVector SpiderFinalLoc = FVector::ZeroVector;
	FVector TimmyFocusPoint = FVector::ZeroVector;
	bool bSpiderPending = false;
	float SpiderPendingLeft = 0.0f;
	bool bSpiderRotationLocked = false;
	FRotator SpiderFinalRot = FRotator::ZeroRotator;

	bool bStarted = false;
	bool bDialogueWaitDone = true;
	bool bMontageDone = false;
	bool bLowHealthDone = false;
	bool bTimmyArrived = false;
	bool bTimmyDialogueStarted = false;
	bool bTimmyWalking = false;
	float TimmyWalkStartZ = 0.0f;
	float TimmyZoomElapsed = 0.0f;
	float TimmyWalkCurSpeed = 220.0f;
	float TimmyWalkTotalDist = 1.0f;
	float TimmyWalkTimeLeft = 0.0f;
	FVector TimmyWalkTarget = FVector::ZeroVector;
	FRotator TimmyWalkFinalRot = FRotator::ZeroRotator;

	UPROPERTY(Transient) TObjectPtr<AActor> Isabella;
	UPROPERTY(Transient) TObjectPtr<AActor> Timmy;
	UPROPERTY(Transient) TObjectPtr<AActor> Spider;

	FDelegateHandle LineFinishedHandle;
	TArray<FTimerHandle> ActiveTimers;
	TSharedPtr<SWidget> EndCardRoot;
	bool bInputLocked = false;
	bool bHasArrowTransform = false;
	FTransform TimmyArrowTransform;
	bool bHasSpiderArrow = false;
	FTransform SpiderArrowTransform;
};
