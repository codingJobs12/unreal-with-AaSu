#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Ability/SOTMSpeedBoostTypes.h"
#include "Ability/SOTMLightningThrowTypes.h"
#include "Objective/SOTMObjectiveSubsystem.h"
#include "SOTMPlayerStateSubsystem.h"
#include "SOTMIngameUIWidget.generated.h"

class AController;
class APawn;
class USOTMBossVitalComponent;
class UBorder;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class USOTMDemoPhase2WorldSubsystem;
class USOTMLightningThrowWorldSubsystem;
class USOTMDemoPhase3WorldSubsystem;
class USOTMDemoPhase4WorldSubsystem;
class ASOTMKeyGateActor;
class USOTMObjectiveSubsystem;
class USOTMPlayerStateSubsystem;

/**
 * Event-driven presentation adapter for the single production gameplay HUD.
 * Gameplay state remains owned by the Player, Coin, and future feature systems.
 */
UCLASS(Abstract, Blueprintable)
class SOTM1_API USOTMIngameUIWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Future Objective System presentation contract. Empty text hides the field. */
	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Objective")
	void SetCurrentObjective(const FText& ObjectiveText);

	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Objective")
	void ClearCurrentObjective();

	/** Future Upgrade System presentation contract. Required <= 0 hides the field. */
	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Upgrade")
	void SetRequiredCoins(int32 CurrentCoins, int32 RequiredCoins);

	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Upgrade")
	void ClearRequiredCoins();

	/** Future Upgrade System presentation contract. Progress is normalized 0..1. */
	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Upgrade")
	void SetUpgradeProgress(float Progress, const FText& DetailText);

	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Upgrade")
	void ClearUpgradeProgress();

	/** Future Boss System presentation contract. Boss UI is hidden by default. */
	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Boss")
	void SetBossProgress(const FText& BossName, float NormalizedHealth);

	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Boss")
	void ClearBossProgress();

	/** Add or update a reusable mission-status row without owning mission state. */
	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Objective")
	void SetMissionTask(FName TaskId, const FText& TaskText, bool bCompleted);

	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Objective")
	void RemoveMissionTask(FName TaskId);

	UFUNCTION(BlueprintCallable, Category="SOTM|HUD|Objective")
	void ClearMissionTasks();

	/** Future cinematic/jump-scare systems may request presentation visibility. */
	UFUNCTION(BlueprintCallable, Category="SOTM|HUD")
	void SetHUDPresentationVisible(bool bVisible);

#if !UE_BUILD_SHIPPING
	void ShowDevelopmentObjectivePreview();
	void ShowDevelopmentUpgradePreview();
	void ShowDevelopmentBossPreview();
	void ClearDevelopmentPreview();
	void CaptureDevelopmentPreview(int32 Width, int32 Height);
#endif

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	UFUNCTION()
	void HandleCoinsChanged(int32 AvailableCoins, int32 LifetimeCoinsCollected);

	UFUNCTION()
	void HandleLivesChanged(int32 CurrentLives, int32 MaximumLives);

	UFUNCTION()
	void HandleObjectiveChanged(FSOTMObjectiveData Objective);

	UFUNCTION()
	void HandleCousinWarningChanged(bool bVisible);

	UFUNCTION()
	void HandleIsabelGateProgressForHUD(bool bReached, bool bHasKey, bool bUnlocked);

	/** Once the gate has been unlocked: key panel is hidden and the player health bar moves bottom-right. */
	void ApplyGateUnlockedHUD();

	UFUNCTION()
	void HandleStationPromptChanged(bool bVisible);

	UFUNCTION()
	void HandleSpeedBoostStateChanged(
		ESOTMSpeedBoostRuntimeState State,
		float RemainingSeconds,
		float NormalizedRemaining);

	UFUNCTION()
	void HandleLightningThrowStateChanged(
		ESOTMLightningThrowRuntimeState State,
		float RemainingSeconds,
		float NormalizedRemaining);

	UFUNCTION()
	void HandlePhase4ProgressChanged(
		bool bChestOpened,
		bool bHasGateKey,
		bool bGateUnlocked,
		bool bDemoCompleted);

	UFUNCTION()
	void HandlePhase4PromptChanged(bool bVisible, FText PromptText);

	UFUNCTION()
	void HandleGatePromptChanged(bool bVisible, FText PromptText);

	UFUNCTION()
	void HandleIsabelBossSpawned(APawn* SpawnedBoss);

	UFUNCTION()
	void HandleIsabelHealthChanged(USOTMBossVitalComponent* VitalComponent, float PreviousHealth, float CurrentHealth, float MaximumHealth);

	UFUNCTION()
	void HandleIsabelDeath(USOTMBossVitalComponent* VitalComponent, AController* InstigatedBy, AActor* DamageCauser);

	UFUNCTION()
	void HandlePhase4Notification(FText Title, FText Detail);

	UFUNCTION()
	void HandlePlayerDeathStarted(AActor* PlayerActor);

	UFUNCTION()
	void HandlePlayerRespawned(AActor* PlayerActor);

	UFUNCTION()
	void HandleGameOver();

	UFUNCTION()
	void HandleInputLocksChanged(bool bInputLocked, TArray<ESOTMInputLockReason> ActiveReasons);

	void EnsureProductionHUD();
	void RefreshCoinCounter(int32 AvailableCoins, int32 LifetimeCoinsCollected, bool bPlayFeedback);
	void RefreshLives(int32 CurrentLives, int32 MaximumLives, bool bPlayFeedback);
	void RefreshObjectivePresentation(const FSOTMObjectiveData& Objective);
	void RefreshPresentationVisibility();
	void FinishCoinPulse();
	void FinishLivesPulse();
	void HidePhase4Notification();
	void HideForestHealthPresentation();

	// --- Designer-bound widgets ---------------------------------------------
	// These are no longer constructed in code (see EnsureProductionHUD in the
	// .cpp). Build a widget of the matching type and EXACT name below inside
	// /Game/UI/Horror/WBP_InGameMain's Designer canvas (NOT the MenuSystemPro
	// example widget WBP_IngameUI under MenuSystemPro/ExampleContent - that
	// one is never spawned at runtime) and UMG will wire the pointer up for
	// you on compile. BindWidgetOptional (rather than BindWidget) means a
	// widget that hasn't been built yet in the Designer is simply left null
	// instead of failing the Blueprint compile, so this can be migrated
	// incrementally. Each widget must be placed DIRECTLY in WBP_InGameMain's
	// own widget tree (not nested inside a separate child UserWidget placed
	// inside it) and must be the exact class listed (e.g. Border, not
	// Overlay/SizeBox/ScaleBox) or the binding silently stays null.
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> ObjectivePanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> CurrentObjectiveSection;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CurrentObjectiveText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ObjectiveProgressText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> FutureObjectivesText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinCounterText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TopRightCoinText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> TopRightCoinPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LivesText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> SpeedBoostPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedBoostText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningThrowText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> LightningThrowProgressBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> SpeedBoostProgressBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> GateKeyPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> GateKeyText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> Phase4PromptPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Phase4PromptText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> Phase4NotificationPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Phase4NotificationTitle;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Phase4NotificationDetail;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> CousinWarningPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> StationPromptPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> RequiredCoinsSection;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RequiredCoinsText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> UpgradeSection;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> UpgradeDetailText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> UpgradeProgressBar;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> MissionTasksHeader;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> MissionTasksContainer;

	// Dynamic per-task rows - NOT a Designer widget, still built in code and
	// parented into MissionTasksContainer above (see SetMissionTask in the .cpp).
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextBlock>> MissionTaskRows;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> BossPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> BossNameText;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> BossProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> BoundPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<USOTMObjectiveSubsystem> BoundObjectiveState;

	UPROPERTY(Transient)
	TObjectPtr<USOTMDemoPhase2WorldSubsystem> BoundPhase2World;

	UPROPERTY(Transient)
	TObjectPtr<USOTMDemoPhase3WorldSubsystem> BoundPhase3World;

	UPROPERTY(Transient)
	TObjectPtr<USOTMLightningThrowWorldSubsystem> BoundLightningWorld;

	UPROPERTY(Transient)
	TObjectPtr<USOTMDemoPhase4WorldSubsystem> BoundPhase4World;

	UPROPERTY(Transient)
	TObjectPtr<ASOTMKeyGateActor> BoundGateActor;

	UPROPERTY(Transient)
	TObjectPtr<USOTMBossVitalComponent> BoundIsabelVital;

	FTimerHandle CoinPulseTimerHandle;
	FTimerHandle LivesPulseTimerHandle;
	FTimerHandle Phase4NotificationTimerHandle;
	int32 DisplayedAvailableCoins = INDEX_NONE;
	int32 DisplayedLives = INDEX_NONE;
	bool bHUDPresentationRequested = true;
	bool bSuppressedByGameplayState = false;
};
