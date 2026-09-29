#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Ability/SOTMSpeedBoostTypes.h"
#include "Ability/SOTMLightningThrowTypes.h"
#include "SOTMPlayerSystemSettings.h"
#include "SOTMPlayerStateSubsystem.generated.h"

class APlayerController;
class IConsoleObject;
class USOTMGameOverWidget;
class USOTMPlayerVitalComponent;
class UUserWidget;

UENUM(BlueprintType)
enum class ESOTMInputLockReason : uint8
{
	Death,
	Respawn,
	Cinematic,
	JumpScare,
	PauseMenu,
	GameOver,
	Custom
};

USTRUCT(BlueprintType)
struct FSOTMCheckpointState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	bool bIsValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName CheckpointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName MapPackageName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FTransform RespawnTransform = FTransform::Identity;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSOTMLivesChangedSignature, int32, CurrentLives, int32, MaximumLives);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSOTMCoinsChangedSignature,
	int32, AvailableCoins,
	int32, LifetimeCoinsCollected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSOTMCoinCollectedSignature,
	int32, CoinValue,
	int32, NewAvailableCoins);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSOTMPlayerActorSignature, AActor*, PlayerActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSOTMCheckpointChangedSignature,
	FName, CheckpointId,
	FName, MapPackageName,
	FTransform, RespawnTransform);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FSOTMInputLocksChangedSignature,
	bool, bInputLocked,
	TArray<ESOTMInputLockReason>, ActiveReasons);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSOTMSimplePlayerStateSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FSOTMPhase4ProgressChangedSignature,
	bool, bChestOpened,
	bool, bHasGateKey,
	bool, bGateUnlocked,
	bool, bDemoCompleted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSOTMIsabelGateProgressChangedSignature,
	bool, bReached,
	bool, bHasKey,
	bool, bUnlocked);

/**
 * Persistent single-player Chapter state. Lives/checkpoints survive map travel
 * because this subsystem belongs to the existing production GameInstance.
 */
UCLASS(BlueprintType)
class SOTM1_API USOTMPlayerStateSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void PrepareForGameplayWorld(FName MapPackageName);
	void BindPlayer(AActor* PlayerActor, USOTMPlayerVitalComponent* VitalComponent);

	UFUNCTION(BlueprintPure, Category="SOTM|Player|State")
	int32 GetCurrentLives() const { return CurrentLives; }

	UFUNCTION(BlueprintPure, Category="SOTM|Player|State")
	int32 GetMaximumLives() const { return MaximumLives; }

	UFUNCTION(BlueprintPure, Category="SOTM|Player|State")
	float GetCurrentHealth() const { return PersistentHealth; }

	UFUNCTION(BlueprintPure, Category="SOTM|Player|State")
	bool IsPlayerDead() const { return bPlayerDead; }

	UFUNCTION(BlueprintPure, Category="SOTM|Player|State")
	bool IsGameOver() const { return bGameOver; }

	UFUNCTION(BlueprintPure, Category="SOTM|Coin")
	int32 GetAvailableCoins() const { return AvailableCoins; }

	UFUNCTION(BlueprintPure, Category="SOTM|Coin")
	int32 GetLifetimeCoinsCollected() const { return LifetimeCoinsCollected; }

	UFUNCTION(BlueprintPure, Category="SOTM|Ability|Speed Boost")
	bool IsSpeedBoostUnlocked() const { return bSpeedBoostUnlocked; }

	UFUNCTION(BlueprintPure, Category="SOTM|Ability|Speed Boost")
	int32 GetSpeedBoostLevel() const { return SpeedBoostLevel; }

	// --- Skill Tree (Ability Points) ------------------------------------------
	// AbilityPoints and SkillUpgradeLevels are persisted (see WriteStateToSaveObject /
	// ReadStateFromSaveObject, schema version 6+) and auto-saved via SavePlayerStateInternal
	// right after every successful TryUnlockSkillUpgrade.
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	int32 GetAbilityPoints() const { return AbilityPoints; }

	// Bought upgrade levels beyond the base unlock: 0 = none, 1 = Level 2 bought,
	// 2 = Level 3 bought, etc. Unknown AbilityId returns 0.
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	int32 GetSkillUpgradeLevel(FName AbilityId) const { return SkillUpgradeLevels.FindRef(AbilityId); }

	// Dev/console utility - grants (or, with a negative Amount, removes) Ability Points
	// and immediately persists them (see SavePlayerStateInternal). Also reachable in-game
	// via the "SOTM.GrantAbilityPoints <amount>" console command.
	UFUNCTION(BlueprintCallable, Category="SOTM|Skill Tree")
	void GrantAbilityPointsForTesting(int32 Amount);

	// Atomic: spends AbilityPointCost points to raise AbilityId from TargetLevel-1 to
	// TargetLevel (TargetLevel must be 2 or higher and exactly one above the current
	// upgrade level - no skipping levels). Returns false and changes nothing on failure.
	UFUNCTION(BlueprintCallable, Category="SOTM|Skill Tree")
	bool TryUnlockSkillUpgrade(FName AbilityId, int32 TargetLevel, int32 AbilityPointCost);

	// Current real gameplay value of each stat, i.e. the ability's base setting plus
	// every upgrade level bought so far's own delta (see FSOTMSkillTreeLevelDefinition).
	// This is what actually drives the ability at runtime now - see
	// USOTMDemoPhase3WorldSubsystem::TryActivateSpeedBoost/FinishActiveSpeedBoost and
	// USOTMLightningThrowWorldSubsystem::TryThrowLightning.
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	float GetEffectiveSpeedBoostDuration() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	float GetEffectiveSpeedBoostCooldown() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	float GetEffectiveLightningThrowCooldown() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	float GetEffectiveLightningThrowRange() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Ability|Lightning Throw")
	bool IsLightningThrowUnlocked() const { return bLightningThrowUnlocked; }

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool IsPhase4ChestOpened() const { return bPhase4ChestOpened; }

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool HasPhase4GateKey() const { return bPhase4HasGateKey; }

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool IsPhase4GateUnlocked() const { return bPhase4GateUnlocked; }

	UFUNCTION(BlueprintPure, Category="SOTM|Demo|Phase 4")
	bool IsPhase4DemoCompleted() const { return bPhase4DemoCompleted; }

	/** Persistent, atomic Phase 4 transactions. Validation remains in the Objective System. */
	bool CommitPhase4ChestOpened();
	bool CommitPhase4GateKey();
	bool CommitPhase4GateUnlocked();
	bool CommitPhase4DemoCompleted();

	/** Standalone Mansion Gate (leads to the Isabel encounter) - separate from Phase 4. */
	UFUNCTION(BlueprintPure, Category="SOTM|Gate|Isabel")
	bool IsIsabelGateReached() const { return bIsabelGateReached; }

	UFUNCTION(BlueprintPure, Category="SOTM|Gate|Isabel")
	bool HasIsabelGateKey() const { return bHasIsabelGateKey; }

	UFUNCTION(BlueprintPure, Category="SOTM|Gate|Isabel")
	bool IsIsabelGateUnlocked() const { return bIsabelGateUnlocked; }

	/** Persistent, atomic Isabel Gate transactions. Validation remains in the Objective System. */
	bool CommitIsabelGateReached();
	bool CommitIsabelGateKey();
	bool CommitIsabelGateUnlocked();

	/** Atomic Phase 3 purchase. Lifetime collection is never reduced. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Ability|Speed Boost")
	ESOTMSpeedBoostPurchaseResult TryPurchaseSpeedBoost(
		int32 UnlockCost,
		bool bCoinObjectiveCompleted);

	/** Chapter 1's second ability. Speed Boost is a hard prerequisite per the design doc. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Ability|Lightning Throw")
	ESOTMLightningThrowPurchaseResult TryPurchaseLightningThrow(
		int32 UnlockCost,
		bool bCoinObjectiveCompleted);

	/** Atomic persistent collection transaction. One stable ID may succeed only once. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Coin")
	bool TryCollectCoin(FGuid PersistentCoinId, AActor* Collector, int32 CoinValue = 1);

	UFUNCTION(BlueprintPure, Category="SOTM|Coin")
	bool IsCoinCollected(FGuid PersistentCoinId) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Coin")
	int32 GetCollectedCoinCount() const { return CollectedCoinIds.Num(); }

	UFUNCTION(BlueprintPure, Category="SOTM|Coin")
	bool IsBoundPlayerActor(const AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Player|Checkpoint")
	FSOTMCheckpointState GetCheckpointState() const { return CheckpointState; }

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Checkpoint")
	bool ActivateCheckpoint(
		FName CheckpointId,
		FName MapPackageName,
		const FTransform& RespawnTransform,
		bool bSaveImmediately = true);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Respawn")
	bool ForceRespawnAtCheckpoint();

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Game Over")
	bool RetryFromGameOver();

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Game Over")
	void ReturnToMainMenu();

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Input")
	void AcquireInputLock(ESOTMInputLockReason Reason);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Input")
	void ReleaseInputLock(ESOTMInputLockReason Reason);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Input")
	void ClearInputLock(ESOTMInputLockReason Reason);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Input")
	void ClearAllInputLocks();

	UFUNCTION(BlueprintPure, Category="SOTM|Player|Input")
	bool HasAnyInputLock() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Player|Input")
	int32 GetInputLockCount(ESOTMInputLockReason Reason) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Player|Input")
	TArray<ESOTMInputLockReason> GetActiveInputLockReasons() const;

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Save")
	bool SavePlayerState();

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Save")
	bool LoadPlayerState(bool bTravelToCheckpoint = false);

	/** Explicit-slot API for safe testing and future save-slot selection UI. */
	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Save")
	bool SavePlayerStateToSlot(const FString& SlotName);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Save")
	bool LoadPlayerStateFromSlot(const FString& SlotName, bool bTravelToCheckpoint = false);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Save")
	void ResetRuntimeStateForNewGame();

	/** One-shot signal raised only when Menu System Pro creates a genuinely new story save. */
	bool ConsumePendingMansionIntro();

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Debug", meta=(DevelopmentOnly))
	void SetLivesForDebug(int32 NewLives);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Debug", meta=(DevelopmentOnly))
	void SetPhase3ProgressForDebug(
		int32 NewAvailableCoins,
		int32 NewLifetimeCoins,
		bool bUnlockSpeedBoost,
		int32 NewSpeedBoostLevel);

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Debug", meta=(DevelopmentOnly))
	void SetPhase4ProgressForDebug(
		bool bChestOpened,
		bool bHasGateKey,
		bool bGateUnlocked,
		bool bDemoCompleted);

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMLivesChangedSignature OnLivesChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Coin|Events")
	FSOTMCoinsChangedSignature OnCoinsChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Coin|Events")
	FSOTMCoinCollectedSignature OnCoinCollected;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Ability|Events")
	FSOTMSpeedBoostOwnershipChangedSignature OnSpeedBoostOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Ability|Lightning Throw")
	FSOTMLightningThrowOwnershipChangedSignature OnLightningThrowOwnershipChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Demo|Phase 4|Events")
	FSOTMPhase4ProgressChangedSignature OnPhase4ProgressChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Gate|Isabel|Events")
	FSOTMIsabelGateProgressChangedSignature OnIsabelGateProgressChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMPlayerActorSignature OnPlayerDeathStarted;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMPlayerActorSignature OnPlayerRespawned;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMSimplePlayerStateSignature OnGameOver;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMCheckpointChangedSignature OnCheckpointChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMInputLocksChangedSignature OnInputLocksChanged;

	static constexpr int32 CurrentSaveVersion = 7;

private:
	UFUNCTION()
	void HandleHealthChanged(
		USOTMPlayerVitalComponent* VitalComponent,
		float PreviousHealth,
		float CurrentHealth,
		float InMaximumHealth);

	UFUNCTION()
	void HandlePlayerDeath(
		USOTMPlayerVitalComponent* VitalComponent,
		AController* InstigatedBy,
		AActor* DamageCauser);

	void ScheduleRespawn();
	void PerformRespawn();
	void ScheduleGameOver();
	void TriggerGameOver();
	void ShowGameOverWidget();
	void HideGameOverWidget();
	void ApplyInputLockState();
	void SyncLegacyCharacterState();
	void EnsureAutomaticMapEntryCheckpoint();
	void ApplyLoadedStateToBoundPlayer();

	bool GetMenuSaveContext(UObject*& OutManager, class USaveGame*& OutSaveObject, FString& OutSlotName) const;
	bool SynchronizeFromActiveMenuSave(const TCHAR* Reason, bool bResetIfSaveHasNoPlayerState);
	bool SavePlayerStateInternal(const TCHAR* Reason);
	bool WriteStateToSaveObject(UObject* SaveObject) const;
	bool ReadStateFromSaveObject(UObject* SaveObject);

	static FName NormalizeMapPackageName(const UWorld* World);

	// Handler for the "SOTM.GrantAbilityPoints <amount>" console command (registered/
	// unregistered in Initialize/Deinitialize). Args[0] is parsed as the integer amount.
	void HandleGrantAbilityPointsCommand(const TArray<FString>& Args);

	// Handler for the "SOTM.GiveIsabelGateKey" testing console command (registered/
	// unregistered in Initialize/Deinitialize) - grants the Mansion Gate key without
	// requiring an in-world key pickup, since none exists yet.
	void HandleGiveIsabelGateKeyCommand(const TArray<FString>& Args);
	void HandleResetIsabelGateCommand(const TArray<FString>& Args);

	// Handler for the "SOTM.GoToIsabelArena" testing console command (registered/
	// unregistered in Initialize/Deinitialize) - teleports the player pawn to the
	// level's actor tagged "IsabelArena", for fast-testing the boss fight without
	// having to walk the whole level each time.
	void HandleGoToIsabelArenaCommand(const TArray<FString>& Args);

	UPROPERTY(Transient)
	int32 CurrentLives = 5;

	UPROPERTY(Transient)
	int32 MaximumLives = 5;

	UPROPERTY(Transient)
	float PersistentHealth = 100.0f;

	UPROPERTY(Transient)
	int32 AvailableCoins = 0;

	UPROPERTY(Transient)
	int32 LifetimeCoinsCollected = 0;

	IConsoleObject* GrantAbilityPointsConsoleCommand = nullptr;
	IConsoleObject* GiveIsabelGateKeyConsoleCommand = nullptr;
	IConsoleObject* ResetIsabelGateConsoleCommand = nullptr;
	IConsoleObject* GoToIsabelArenaConsoleCommand = nullptr;

	UPROPERTY(Transient)
	bool bSpeedBoostUnlocked = false;

	bool bLightningThrowUnlocked = false;

	UPROPERTY(Transient)
	int32 SpeedBoostLevel = 0;

	UPROPERTY(Transient)
	int32 AbilityPoints = 0;

	UPROPERTY(Transient)
	TMap<FName, int32> SkillUpgradeLevels;

	UPROPERTY(Transient)
	bool bPhase4ChestOpened = false;

	UPROPERTY(Transient)
	bool bPhase4HasGateKey = false;

	UPROPERTY(Transient)
	bool bPhase4GateUnlocked = false;

	UPROPERTY(Transient)
	bool bPhase4DemoCompleted = false;

	UPROPERTY(Transient)
	bool bIsabelGateReached = false;

	UPROPERTY(Transient)
	bool bHasIsabelGateKey = false;

	UPROPERTY(Transient)
	bool bIsabelGateUnlocked = false;

	/** Stable identities of placed Coins already collected in this Chapter run. */
	TSet<FGuid> CollectedCoinIds;

	bool bCoinStateDirty = false;

	UPROPERTY(Transient)
	bool bPlayerDead = false;

	UPROPERTY(Transient)
	bool bGameOver = false;

	UPROPERTY(Transient)
	bool bDeathProcessing = false;

	UPROPERTY(Transient)
	bool bAutoLoadAttempted = false;

	/** Menu System Pro slot/object already deserialized into this runtime subsystem. */
	FString LastSynchronizedSaveSlot;
	TWeakObjectPtr<class USaveGame> LastSynchronizedSaveObject;

	UPROPERTY(Transient)
	bool bLoadedPlayerState = false;

	/** New Game route gate. Continue/load must never replay the Mansion introduction. */
	bool bPendingMansionIntro = false;

	UPROPERTY(Transient)
	bool bHasInitializedPlayerHealth = false;

	UPROPERTY(Transient)
	bool bPendingRespawnAfterTravel = false;

	UPROPERTY(Transient)
	FName CurrentGameplayMap = NAME_None;

	UPROPERTY(Transient)
	FSOTMCheckpointState CheckpointState;

	UPROPERTY(Transient)
	TObjectPtr<AActor> BoundPlayerActor;

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerVitalComponent> BoundVitalComponent;

	UPROPERTY(Transient)
	TMap<ESOTMInputLockReason, int32> InputLockCounts;

	TWeakObjectPtr<APlayerController> InputLockedController;
	bool bInputBlockApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<USOTMGameOverWidget> GameOverWidget;

	FTimerHandle RespawnTimerHandle;
	FTimerHandle GameOverTimerHandle;
};
