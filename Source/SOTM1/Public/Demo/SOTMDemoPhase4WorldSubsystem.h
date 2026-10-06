#pragma once

#include "CoreMinimal.h"
#include "Demo/SOTMPhase4Types.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "Components/SlateWrapperTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SOTMDemoPhase4WorldSubsystem.generated.h"

class AStaticMeshActor;
class ASOTMChestActor;
class ASOTMPhase4Interactable;
class UAudioComponent;
class ALevelSequenceActor;
class ULevelSequencePlayer;
class UWidgetComponent;
class UEnhancedInputComponent;
class UUserWidget;
class UInputAction;
class USOTMDemoCompleteWidget;
class USOTMObjectiveSubsystem;
class USOTMPlayerStateSubsystem;

/** CH1-only Phase 4 chest, key, gate and safe demo-ending coordinator. */
UCLASS()
class SOTM1_API USOTMDemoPhase4WorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Demo|Phase 4")
	FSOTMPhase4PromptChangedSignature OnPromptChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Demo|Phase 4")
	FSOTMPhase4NotificationSignature OnNotification;

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool IsPlayerNearChest() const { return bNearChest; }

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool IsPlayerNearGate() const { return bNearGate; }

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool IsChestDialogueActive() const { return bChestDialogueActive; }

	/** Called by ASOTMKeyGateActor when the player unlocks the Isabel gate with the key:
	 * IsabelaIntro_sequence + Isabella's three lines. The Isabel fight continues afterwards
	 * (no Demo Complete screen is triggered from this entry point). */
	void PlayIsabelGateIntro();

#if !UE_BUILD_SHIPPING
	void BeginDevelopmentAcceptanceRoute();
	void BeginDevelopmentDeathAfterKeyAcceptance();
	void RunDevelopmentDataAcceptance();
#endif

private:
	void InitializePhase4();
	void FindProductionArtAndCreateAnchors();
	void BindProductionInput();
	void UnbindProductionInput();
	void HandleInteractInput();
	void HandleEntered(ESOTMPhase4InteractableKind Kind, AActor* Actor);
	void HandleExited(ESOTMPhase4InteractableKind Kind, AActor* Actor);
	void RefreshPrompt();
	void InteractWithChest();
	void InteractWithGate();
	void InteractWithKey();
	void BeginChestPresentation(bool bRestoreImmediately);
	void UpdateChestPresentation();
	void BeginGatePresentation(bool bRestoreImmediately);
	void UpdateGatePresentation();
	void FinishGatePresentation();
	void ShowDemoComplete();

	// Timmy's chest line: plays when the chest is opened, hides every UI element, locks
	// all input, silences Cousin voices/subtitles, and types the subtitle in sync with
	// the VO (same behaviour as the Phase 3 upgrade-unlock dialogue).
	void BeginChestDialogue();
	void PlayChestDialogueLine();
	void StartChestDialogueTypewriter(float TargetDuration);
	void TickChestDialogueTypewriter();
	void EndChestDialogue();

	// Isabella gate cutscene: IsabelaIntro_sequence starts when the gate is unlocked; after
	// 4 seconds Isabella's three lines play (separate VO files, typed subtitles). UI hidden,
	// input locked and Cousins silenced until the sequence AND the dialogue are both done;
	// the Demo Complete screen follows afterwards.
	void BeginIsabelCutscene();
	void StartIsabelDialogue();
	void PlayIsabelLine(int32 Index);
	void PlayNextIsabelLine();
	void TickIsabelTypewriter();
	void FinishIsabelLine();
	void FinishIsabelDialogue();
	void EndIsabelCutscene(bool bAborted);
	void HideAllUIWidgets(TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& OutHidden);
	void RestoreHiddenUIWidgets(TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility>& Hidden);
	UFUNCTION()
	void HandleIsabelLineAudioFinished();
	UFUNCTION()
	void HandleIsabelSequenceFinished();
	// Ends only the spoken line + subtitle; the whole chest cutscene state ends once the
	// sequence has finished as well (see HandleChestSequenceFinished / EndChestDialogue).
	void FinishChestDialogueLine();
	bool IsPlayerWithinChestRange() const;
	void ResyncChestProximity();

	// Chest marker (the WidgetComponent inside BP_Chest): shown only while the active
	// objective is "Find the chest", removed for good once the player reaches the chest.
	void RefreshChestMarker();
	UFUNCTION()
	void HandleObjectiveChanged(FSOTMObjectiveData Objective);

	// Chest_Sequence: plays right after the chest opens; the dialogue starts when it ends.
	UFUNCTION()
	void HandleChestSequenceFinished();
	UFUNCTION()
	void HandleChestDialogueAudioFinished();
	UFUNCTION()
	void HideDemoComplete();

	UFUNCTION()
	void HandlePlayerUnavailable(AActor* PlayerActor);

	UFUNCTION()
	void HandlePlayerRespawned(AActor* PlayerActor);

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> PlayerState;

	UPROPERTY(Transient)
	TObjectPtr<USOTMObjectiveSubsystem> Objectives;

	UPROPERTY(Transient)
	TObjectPtr<ASOTMChestActor> ChestArt;

	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> GateArt;

	UPROPERTY(Transient)
	TObjectPtr<AStaticMeshActor> KeyPresentation;

	UPROPERTY(Transient)
	TObjectPtr<ASOTMPhase4Interactable> ChestAnchor;

	UPROPERTY(Transient)
	TObjectPtr<ASOTMPhase4Interactable> GateAnchor;

	UPROPERTY(Transient)
	TObjectPtr<ASOTMPhase4Interactable> KeyAnchor;

	UPROPERTY(Transient)
	TObjectPtr<USOTMDemoCompleteWidget> DemoCompleteWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractInputAction;

	TWeakObjectPtr<UEnhancedInputComponent> BoundEnhancedInput;
	uint32 InteractBindingHandle = 0;
	bool bNearChest = false;
	bool bNearGate = false;
	bool bNearKey = false;
	bool bGateAnimationRunning = false;
	bool bDemoInputLockHeld = false;

	bool bChestDialogueActive = false;
	bool bChestMarkerDismissed = false;

	bool bIsabelCutsceneActive = false;
	bool bIsabelFromKeyGate = false;
	bool bIsabelSequencePlaying = false;
	bool bIsabelDialogueDone = false;
	bool bIsabelInputLockHeld = false;
	bool bPreviousMouseCursorIsabel = false;
	int32 IsabelLineIndex = 0;
	int32 IsabelRevealedChars = 0;
	FString IsabelFullLine;
	TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> IsabelHiddenWidgets;
	TSharedPtr<class SWidget> IsabelSubtitleRoot;
	TSharedPtr<class STextBlock> IsabelSubtitleLine;
	FTimerHandle IsabelDialogueStartTimer;
	FTimerHandle IsabelLineTimeoutTimer;
	FTimerHandle IsabelTypewriterTimer;
	FTimerHandle IsabelLineGapTimer;
	FTimerHandle IsabelSequenceTimeoutTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> IsabelVoice;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> IsabelSequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> IsabelSequenceActor;
	bool bChestSequencePlaying = false;
	bool bChestLineDone = false;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetComponent> ChestMarker;

	UPROPERTY(Transient)
	TObjectPtr<ULevelSequencePlayer> ChestSequencePlayer;

	UPROPERTY(Transient)
	TObjectPtr<ALevelSequenceActor> ChestSequenceActor;

	FTimerHandle ChestSequenceTimeoutTimer;
	bool bChestDialogueInputLockHeld = false;
	bool bPreviousMouseCursorChestDialogue = false;
	TMap<TWeakObjectPtr<UUserWidget>, ESlateVisibility> ChestDialogueHiddenWidgets;
	TSharedPtr<class SWidget> ChestDialogueSubtitleRoot;
	TSharedPtr<class STextBlock> ChestDialogueLineText;
	FString ChestDialogueFullLine;
	int32 ChestDialogueRevealedChars = 0;
	FTimerHandle ChestDialoguePreDelayTimer;
	FTimerHandle ChestDialogueTimeoutTimer;
	FTimerHandle ChestDialogueTypewriterTimer;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ChestDialogueAudio;
	float ChestAnimationAlpha = 0.0f;
	float GateAnimationAlpha = 0.0f;
	FTransform ChestClosedTransform = FTransform::Identity;
	FTransform GateClosedTransform = FTransform::Identity;
	FTimerHandle InitializeTimer;
	FTimerHandle ChestAnimationTimer;
	FTimerHandle GateAnimationTimer;
	FTimerHandle DemoCompleteTimer;
};
