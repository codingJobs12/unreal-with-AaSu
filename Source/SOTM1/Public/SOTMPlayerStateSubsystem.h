#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SOTMPlayerSystemSettings.h"
#include "SOTMPlayerStateSubsystem.generated.h"

class APlayerController;
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

	UFUNCTION(BlueprintCallable, Category="SOTM|Player|Debug", meta=(DevelopmentOnly))
	void SetLivesForDebug(int32 NewLives);

	UPROPERTY(BlueprintAssignable, Category="SOTM|Player|Events")
	FSOTMLivesChangedSignature OnLivesChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Coin|Events")
	FSOTMCoinsChangedSignature OnCoinsChanged;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Coin|Events")
	FSOTMCoinCollectedSignature OnCoinCollected;

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

	static constexpr int32 CurrentSaveVersion = 2;

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
