#pragma once

#include "CoreMinimal.h"
#include "Ability/SOTMSpeedBoostTypes.h"
#include "Ability/SOTMLightningThrowTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "SOTMDemoPhase3WorldSubsystem.generated.h"

class ASOTMTimmyUpgradeStation;
class UCharacterMovementComponent;
class UAudioComponent;
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class USOTMPlayerStateSubsystem;
class USOTMSkillTreeWidget;
class USOTMSpeedBoostComponent;
class USOTMUpgradeStationWidget;

/** CH1-only Phase 3 station, purchase, input and runtime Speed Boost coordinator. */
UCLASS()
class SOTM1_API USOTMDemoPhase3WorldSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Phase 3")
	FSOTMStationPromptChangedSignature OnStationPromptChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Phase 3")
	FSOTMSpeedBoostStateChangedSignature OnSpeedBoostStateChanged;

	UFUNCTION(BlueprintPure, Category="SOTM|Phase 3")
	ESOTMSpeedBoostRuntimeState GetSpeedBoostState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category="SOTM|Phase 3")
	bool IsPlayerInStationRange() const { return bPlayerInStationRange; }

	UFUNCTION(BlueprintPure, Category="SOTM|Phase 3")
	bool IsUpgradeUIOpen() const { return UpgradeWidget != nullptr; }

	UFUNCTION(BlueprintPure, Category="SOTM|Phase 3")
	bool IsSkillTreeUIOpen() const { return SkillTreeWidget != nullptr; }

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	ESOTMSpeedBoostPurchaseResult TryPurchaseSpeedBoost();

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	ESOTMLightningThrowPurchaseResult TryPurchaseLightningThrow();

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	bool TryActivateSpeedBoost();

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	void OpenUpgradeUI();

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	void CloseUpgradeUI();

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	void OpenSkillTreeUI();

	UFUNCTION(BlueprintCallable, Category="SOTM|Phase 3")
	void CloseSkillTreeUI();

private:
	void InitializePhase3();
	void BindToPlacedTimmyStation();
	void BindProductionInput();
	void UnbindProductionInput();
	void BlockShiftTestSprint(class UEnhancedInputLocalPlayerSubsystem* InputSubsystem);
	void HandleInteractInput();
	void HandleSpeedBoostInput();
	void HandleSkillTreeInput();
	void HandleStationEntered(AActor* Actor);
	void HandleStationExited(AActor* Actor);
	void SetRuntimeState(ESOTMSpeedBoostRuntimeState NewState, float RemainingSeconds = 0.0f);
	void UpdateRuntimePresentation();
	void FinishActiveSpeedBoost();
	void FinishCooldown();
	void RestoreMovementSpeed();
	void ResetRuntimeAfterDeath();
	UCharacterMovementComponent* ResolveMovementComponent() const;

#if !UE_BUILD_SHIPPING
	void BeginDevelopmentAcceptanceRoute();
	void BeginDevelopmentPersistenceCheck();
	void PositionForDevelopmentCousinChase();
	void LogDevelopmentAcceptanceState(const TCHAR* Label) const;
	void CaptureDevelopmentEvidence(const FString& Label) const;
#endif

	UFUNCTION()
	void HandleOwnershipChanged(bool bUnlocked, int32 Level);

	UFUNCTION()
	void HandlePlayerDeathStarted(AActor* PlayerActor);

	UFUNCTION()
	void HandlePlayerRespawned(AActor* PlayerActor);

	UFUNCTION()
	void HandleGameOver();

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> PlayerState;

	UPROPERTY(Transient)
	TObjectPtr<ASOTMTimmyUpgradeStation> StationActor;

	UPROPERTY(Transient)
	TObjectPtr<USOTMUpgradeStationWidget> UpgradeWidget;

	UPROPERTY(Transient)
	TObjectPtr<USOTMSkillTreeWidget> SkillTreeWidget;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SpeedBoostInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SkillTreeInputAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> Phase3InputContext;

	/** Higher-priority context that swallows the Shift test sprint keys (see BlockShiftTestSprint). */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> SprintBlockContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintBlockAction;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveBoostAudio;

	TWeakObjectPtr<UEnhancedInputComponent> BoundEnhancedInput;
	uint32 InteractBindingHandle = 0;
	uint32 SpeedBoostBindingHandle = 0;
	uint32 SkillTreeBindingHandle = 0;
	bool bPlayerInStationRange = false;
	bool bUpgradeInputLockHeld = false;
	bool bPreviousMouseCursor = false;
	bool bSkillTreeInputLockHeld = false;
	bool bPreviousMouseCursorSkillTree = false;
	ESOTMSpeedBoostRuntimeState RuntimeState = ESOTMSpeedBoostRuntimeState::Locked;
	/** Pawn component that applies the movement effect while Active. */
	TWeakObjectPtr<USOTMSpeedBoostComponent> ActiveBoostComponent;
	FTimerHandle InitializeTimer;
	FTimerHandle ActiveTimer;
	FTimerHandle CooldownTimer;
	FTimerHandle PresentationTimer;

	// Mini scripted "beat" played the moment an upgrade (Speed Boost / Lightning
	// Throw) is unlocked: hides the gameplay HUD, switches to UI-only input (so
	// the player can't act mid-line), waits a short beat, then shows a raw-Slate
	// subtitle synced to that line's VO duration (falls back to a fixed timer if
	// the VO asset is missing), and restores everything once the line ends.
	void BeginUpgradeUnlockDialogue(const TCHAR* VOPath, const FText& Speaker, const FText& Line);
	void HideGameplayUIForDialogue();
	void PlayPendingUpgradeDialogueLine();
	void EndUpgradeUnlockDialogue();

	UFUNCTION()
	void HandleUpgradeDialogueAudioFinished();

	// Reveals UpgradeDialogueFullLine into UpgradeDialogueLineText one character
	// at a time (typewriter effect), paced so it finishes roughly with the VO.
	void StartUpgradeDialogueTypewriter(float TargetDuration);
	void TickUpgradeDialogueTypewriter();

	TSharedPtr<class SWidget> UpgradeDialogueSubtitleRoot;
	TSharedPtr<class STextBlock> UpgradeDialogueSpeakerText;
	TSharedPtr<class STextBlock> UpgradeDialogueLineText;
	FTimerHandle UpgradeDialoguePreDelayTimer;
	FTimerHandle UpgradeDialogueTimeoutTimer;
	FTimerHandle UpgradeDialogueTypewriterTimer;
	FString UpgradeDialogueFullLine;
	int32 UpgradeDialogueRevealedChars = 0;
	float UpgradeDialogueTypewriterInterval = 0.045f;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> UpgradeDialogueAudio;

	// Pending line, set by BeginUpgradeUnlockDialogue and consumed by
	// PlayPendingUpgradeDialogueLine once the pre-delay elapses.
	const TCHAR* PendingDialogueVOPath = nullptr;
	FText PendingDialogueSpeaker;
	FText PendingDialogueLine;

	// Gameplay HUD widgets hidden for the duration of the dialogue, with their
	// original visibility so they can be restored exactly as they were.
	TMap<TWeakObjectPtr<class UUserWidget>, ESlateVisibility> HiddenGameplayUIWidgets;
	bool bUpgradeDialogueInputLockHeld = false;
	bool bPreviousMouseCursorUpgradeDialogue = false;
};
