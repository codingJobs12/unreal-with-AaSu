#include "SOTMPlayerStateSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Components/BoxComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SaveGame.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Ability/SOTMLightningThrowSettings.h"
#include "Ability/SOTMPhase3Settings.h"
#include "Ability/SOTMSkillTreeSettings.h"
#include "SOTMGameOverWidget.h"
#include "SOTMPlayerVitalComponent.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace SOTMPlayerStatePrivate
{
	const FName HasStateProperty(TEXT("SOTM_HasPlayerState"));
	const FName SaveVersionProperty(TEXT("SOTM_SaveVersion"));
	const FName CurrentLivesProperty(TEXT("SOTM_CurrentLives"));
	const FName MaximumLivesProperty(TEXT("SOTM_MaximumLives"));
	const FName CurrentHealthProperty(TEXT("SOTM_CurrentHealth"));
	const FName CheckpointIdProperty(TEXT("SOTM_CheckpointId"));
	const FName CheckpointMapProperty(TEXT("SOTM_CheckpointMap"));
	const FName CheckpointTransformProperty(TEXT("SOTM_CheckpointTransform"));
	const FName AvailableCoinsProperty(TEXT("SOTM_AvailableCoins"));
	const FName LifetimeCoinsCollectedProperty(TEXT("SOTM_LifetimeCoinsCollected"));
	const FName CollectedCoinIdsProperty(TEXT("SOTM_CollectedCoinIds"));
	const FString SpeedBoostRecordPrefix(TEXT("SOTM_SPEEDBOOST|"));
	const FString Phase4RecordPrefix(TEXT("SOTM_PHASE4|"));
	const FString LightningThrowRecordPrefix(TEXT("SOTM_LIGHTNING|"));
	const FString SkillTreeRecordPrefix(TEXT("SOTM_SKILLTREE|"));
	const FString IsabelGateRecordPrefix(TEXT("SOTM_ISABELGATE|"));

	bool GetBoolProperty(const UObject* Object, const FName Name, bool& OutValue)
	{
		if (const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name))
		{
			OutValue = Property->GetPropertyValue_InContainer(Object);
			return true;
		}
		return false;
	}

	bool SetBoolProperty(UObject* Object, const FName Name, const bool Value)
	{
		if (FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
			return true;
		}
		return false;
	}

	bool GetIntProperty(const UObject* Object, const FName Name, int32& OutValue)
	{
		if (const FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), Name))
		{
			OutValue = Property->GetPropertyValue_InContainer(Object);
			return true;
		}
		return false;
	}

	bool SetIntProperty(UObject* Object, const FName Name, const int32 Value)
	{
		if (FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
			return true;
		}
		return false;
	}

	bool GetFloatProperty(const UObject* Object, const FName Name, float& OutValue)
	{
		const FNumericProperty* NumericProperty =
			CastField<FNumericProperty>(FindFProperty<FProperty>(Object->GetClass(), Name));
		if (!NumericProperty)
		{
			return false;
		}

		const void* ValueAddress = NumericProperty->ContainerPtrToValuePtr<void>(Object);
		if (NumericProperty->IsFloatingPoint())
		{
			OutValue = static_cast<float>(NumericProperty->GetFloatingPointPropertyValue(ValueAddress));
		}
		else
		{
			OutValue = static_cast<float>(NumericProperty->GetSignedIntPropertyValue(ValueAddress));
		}
		return true;
	}

	bool SetFloatProperty(UObject* Object, const FName Name, const float Value)
	{
		FNumericProperty* NumericProperty =
			CastField<FNumericProperty>(FindFProperty<FProperty>(Object->GetClass(), Name));
		if (!NumericProperty)
		{
			return false;
		}

		void* ValueAddress = NumericProperty->ContainerPtrToValuePtr<void>(Object);
		if (NumericProperty->IsFloatingPoint())
		{
			NumericProperty->SetFloatingPointPropertyValue(ValueAddress, static_cast<double>(Value));
		}
		else
		{
			NumericProperty->SetIntPropertyValue(ValueAddress, FMath::RoundToInt64(Value));
		}
		return true;
	}

	bool GetNameProperty(const UObject* Object, const FName Name, FName& OutValue)
	{
		if (const FNameProperty* Property = FindFProperty<FNameProperty>(Object->GetClass(), Name))
		{
			OutValue = Property->GetPropertyValue_InContainer(Object);
			return true;
		}
		return false;
	}

	bool SetNameProperty(UObject* Object, const FName Name, const FName Value)
	{
		if (FNameProperty* Property = FindFProperty<FNameProperty>(Object->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
			return true;
		}
		return false;
	}

	bool GetTransformProperty(const UObject* Object, const FName Name, FTransform& OutValue)
	{
		if (const FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), Name))
		{
			if (Property->Struct == TBaseStructure<FTransform>::Get())
			{
				OutValue = *Property->ContainerPtrToValuePtr<FTransform>(Object);
				return true;
			}
		}
		return false;
	}

	bool SetTransformProperty(UObject* Object, const FName Name, const FTransform& Value)
	{
		if (FStructProperty* Property = FindFProperty<FStructProperty>(Object->GetClass(), Name))
		{
			if (Property->Struct == TBaseStructure<FTransform>::Get())
			{
				*Property->ContainerPtrToValuePtr<FTransform>(Object) = Value;
				return true;
			}
		}
		return false;
	}

	bool GetStringProperty(const UObject* Object, const FName Name, FString& OutValue)
	{
		if (const FStrProperty* Property = FindFProperty<FStrProperty>(Object->GetClass(), Name))
		{
			OutValue = Property->GetPropertyValue_InContainer(Object);
			return true;
		}
		return false;
	}

	bool SetStringProperty(UObject* Object, const FName Name, const FString& Value)
	{
		if (FStrProperty* Property = FindFProperty<FStrProperty>(Object->GetClass(), Name))
		{
			Property->SetPropertyValue_InContainer(Object, Value);
			return true;
		}
		return false;
	}
}

void USOTMPlayerStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	MaximumLives = FMath::Max(1, Settings->MaximumLives);
	CurrentLives = FMath::Clamp(Settings->StartingLives, 1, MaximumLives);
	PersistentHealth = FMath::Max(1.0f, Settings->MaximumHealth);

	GrantAbilityPointsConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SOTM.GrantAbilityPoints"),
		TEXT("Grants (or, with a negative amount, removes) Ability Points and immediately saves them, e.g. 'SOTM.GrantAbilityPoints 50'."),
		FConsoleCommandWithArgsDelegate::CreateUObject(
			this, &USOTMPlayerStateSubsystem::HandleGrantAbilityPointsCommand));

	GiveIsabelGateKeyConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SOTM.GiveIsabelGateKey"),
		TEXT("Testing only: grants the Mansion Gate key (no in-world key pickup exists yet), e.g. 'SOTM.GiveIsabelGateKey'."),
		FConsoleCommandWithArgsDelegate::CreateUObject(
			this, &USOTMPlayerStateSubsystem::HandleGiveIsabelGateKeyCommand));

	ResetIsabelGateConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SOTM.ResetIsabelGate"),
		TEXT("Testing only: clears the Mansion Gate's reached/key/unlocked progress and saves, so you can replay the overlap->unlock flow, e.g. 'SOTM.ResetIsabelGate'."),
		FConsoleCommandWithArgsDelegate::CreateUObject(
			this, &USOTMPlayerStateSubsystem::HandleResetIsabelGateCommand));

	GoToIsabelArenaConsoleCommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("SOTM.GoToIsabelArena"),
		TEXT("Testing only: teleports the player near the level's actor tagged 'IsabelArena', e.g. 'SOTM.GoToIsabelArena'."),
		FConsoleCommandWithArgsDelegate::CreateUObject(
			this, &USOTMPlayerStateSubsystem::HandleGoToIsabelArenaCommand));
}

void USOTMPlayerStateSubsystem::Deinitialize()
{
	if (GrantAbilityPointsConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(GrantAbilityPointsConsoleCommand);
		GrantAbilityPointsConsoleCommand = nullptr;
	}

	if (GiveIsabelGateKeyConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(GiveIsabelGateKeyConsoleCommand);
		GiveIsabelGateKeyConsoleCommand = nullptr;
	}
	if (ResetIsabelGateConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(ResetIsabelGateConsoleCommand);
		ResetIsabelGateConsoleCommand = nullptr;
	}
	if (GoToIsabelArenaConsoleCommand)
	{
		IConsoleManager::Get().UnregisterConsoleObject(GoToIsabelArenaConsoleCommand);
		GoToIsabelArenaConsoleCommand = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
		World->GetTimerManager().ClearTimer(GameOverTimerHandle);
	}

	if (BoundVitalComponent)
	{
		BoundVitalComponent->OnHealthChanged.RemoveDynamic(this, &USOTMPlayerStateSubsystem::HandleHealthChanged);
		BoundVitalComponent->OnDeath.RemoveDynamic(this, &USOTMPlayerStateSubsystem::HandlePlayerDeath);
	}

	ClearAllInputLocks();
	HideGameOverWidget();
	Super::Deinitialize();
}

void USOTMPlayerStateSubsystem::PrepareForGameplayWorld(const FName MapPackageName)
{
	CurrentGameplayMap = MapPackageName;

	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	// The production menu chooses/loads its authoritative save only after New Game
	// or Continue is pressed. Consuming the one-time auto-load in Main_Menu_Map read
	// the manager's placeholder object (and its default "Slot 1") too early.
	if (MapPackageName == Settings->MainMenuMap)
	{
		return;
	}

	if (Settings->bAutoLoadPlayerState)
	{
		SynchronizeFromActiveMenuSave(TEXT("GameplayWorld"), true);
	}
}

void USOTMPlayerStateSubsystem::BindPlayer(AActor* PlayerActor, USOTMPlayerVitalComponent* VitalComponent)
{
	if (!IsValid(PlayerActor) || !IsValid(VitalComponent))
	{
		return;
	}

	if (BoundVitalComponent && BoundVitalComponent != VitalComponent)
	{
		BoundVitalComponent->OnHealthChanged.RemoveDynamic(this, &USOTMPlayerStateSubsystem::HandleHealthChanged);
		BoundVitalComponent->OnDeath.RemoveDynamic(this, &USOTMPlayerStateSubsystem::HandlePlayerDeath);
	}

	// Continue finishes loading through Menu System Pro's loading-screen callback.
	// Re-check at gameplay-pawn binding so the HUD/vitals cannot bind to the
	// pre-Continue placeholder state if World BeginPlay happened first. The menu
	// preview pawn must never be interpreted as a New Game.
	if (CurrentGameplayMap != GetDefault<USOTMPlayerSystemSettings>()->MainMenuMap)
	{
		SynchronizeFromActiveMenuSave(TEXT("PlayerBind"), true);
	}

	BoundPlayerActor = PlayerActor;
	BoundVitalComponent = VitalComponent;

	BoundVitalComponent->OnHealthChanged.RemoveDynamic(this, &USOTMPlayerStateSubsystem::HandleHealthChanged);
	BoundVitalComponent->OnDeath.RemoveDynamic(this, &USOTMPlayerStateSubsystem::HandlePlayerDeath);
	BoundVitalComponent->OnHealthChanged.AddDynamic(this, &USOTMPlayerStateSubsystem::HandleHealthChanged);
	BoundVitalComponent->OnDeath.AddDynamic(this, &USOTMPlayerStateSubsystem::HandlePlayerDeath);

	ApplyLoadedStateToBoundPlayer();
	EnsureAutomaticMapEntryCheckpoint();
	ApplyInputLockState();

	if (bPendingRespawnAfterTravel)
	{
		bPendingRespawnAfterTravel = false;
		PerformRespawn();
	}
}

bool USOTMPlayerStateSubsystem::TryCollectCoin(
	const FGuid PersistentCoinId,
	AActor* Collector,
	const int32 CoinValue)
{
	if (!PersistentCoinId.IsValid() ||
		CollectedCoinIds.Contains(PersistentCoinId) ||
		!IsBoundPlayerActor(Collector) ||
		CoinValue <= 0 ||
		bPlayerDead ||
		bGameOver)
	{
		return false;
	}

	CollectedCoinIds.Add(PersistentCoinId);
	AvailableCoins = FMath::Max(0, AvailableCoins + CoinValue);

	// Every 10 lifetime coins collected grants 1 Ability Point. Comparing the
	// "tens" bucket before/after (rather than just checking `% 10 == 0`) means
	// a multi-value pickup that crosses more than one multiple of 10 in a
	// single collect still grants the correct number of points.
	const int32 PreviousLifetimeCoins = LifetimeCoinsCollected;
	LifetimeCoinsCollected = FMath::Max(0, LifetimeCoinsCollected + CoinValue);
	const int32 AbilityPointsEarned = (LifetimeCoinsCollected / 10) - (PreviousLifetimeCoins / 10);
	if (AbilityPointsEarned > 0)
	{
		AbilityPoints += AbilityPointsEarned;
	}

	bCoinStateDirty = true;
	OnCoinCollected.Broadcast(CoinValue, AvailableCoins);
	OnCoinsChanged.Broadcast(AvailableCoins, LifetimeCoinsCollected);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("SOTM Coin: collected id=%s value=%d available=%d lifetime=%d abilityPointsEarned=%d totalAbilityPoints=%d"),
		*PersistentCoinId.ToString(EGuidFormats::DigitsWithHyphens),
		CoinValue,
		AvailableCoins,
		LifetimeCoinsCollected,
		AbilityPointsEarned,
		AbilityPoints);

	// Persist only after the authoritative transaction has succeeded. SavePlayerState
	// resolves the active Menu System Pro slot used by Continue, so a player may quit
	// immediately after a pickup without losing the new total or collected Coin ID.
	if (!SavePlayerStateInternal(TEXT("CoinPickup")))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("SOTM Coin: autosave failed after collecting id=%s; runtime state remains valid but is not guaranteed on relaunch."),
			*PersistentCoinId.ToString(EGuidFormats::DigitsWithHyphens));
	}
	return true;
}

ESOTMSpeedBoostPurchaseResult USOTMPlayerStateSubsystem::TryPurchaseSpeedBoost(
	const int32 UnlockCost,
	const bool bCoinObjectiveCompleted)
{
	if (bSpeedBoostUnlocked || SpeedBoostLevel > 0)
	{
		return ESOTMSpeedBoostPurchaseResult::AlreadyOwned;
	}
	if (!bCoinObjectiveCompleted)
	{
		return ESOTMSpeedBoostPurchaseResult::ObjectiveIncomplete;
	}
	if (UnlockCost <= 0)
	{
		return ESOTMSpeedBoostPurchaseResult::InvalidCost;
	}
	if (AvailableCoins < UnlockCost)
	{
		return ESOTMSpeedBoostPurchaseResult::NotEnoughCoins;
	}

	UObject* Manager = nullptr;
	USaveGame* SaveObject = nullptr;
	FString SlotName;
	if (!GetMenuSaveContext(Manager, SaveObject, SlotName) || SlotName.IsEmpty())
	{
		return ESOTMSpeedBoostPurchaseResult::NoActiveSave;
	}

	const int32 PreviousAvailableCoins = AvailableCoins;
	AvailableCoins -= UnlockCost;
	bSpeedBoostUnlocked = true;
	SpeedBoostLevel = 1;

	// Persist the complete transaction before announcing it. If the production
	// slot cannot save, roll back every runtime field so a failed click is atomic.
	if (!SavePlayerStateInternal(TEXT("SpeedBoostPurchase")))
	{
		AvailableCoins = PreviousAvailableCoins;
		bSpeedBoostUnlocked = false;
		SpeedBoostLevel = 0;
		return ESOTMSpeedBoostPurchaseResult::SaveFailed;
	}

	OnCoinsChanged.Broadcast(AvailableCoins, LifetimeCoinsCollected);
	OnSpeedBoostOwnershipChanged.Broadcast(bSpeedBoostUnlocked, SpeedBoostLevel);
	UE_LOG(LogTemp, Display,
		TEXT("SOTM Speed Boost purchased slot=\"%s\" cost=%d available=%d lifetime=%d level=%d"),
		*SlotName, UnlockCost, AvailableCoins, LifetimeCoinsCollected, SpeedBoostLevel);
	return ESOTMSpeedBoostPurchaseResult::Success;
}

ESOTMLightningThrowPurchaseResult USOTMPlayerStateSubsystem::TryPurchaseLightningThrow(
	const int32 UnlockCost,
	const bool bCoinObjectiveCompleted)
{
	if (bLightningThrowUnlocked)
	{
		return ESOTMLightningThrowPurchaseResult::AlreadyOwned;
	}
	if (!bCoinObjectiveCompleted)
	{
		return ESOTMLightningThrowPurchaseResult::ObjectiveIncomplete;
	}
	if (!bSpeedBoostUnlocked)
	{
		return ESOTMLightningThrowPurchaseResult::SpeedBoostMissing;
	}
	if (UnlockCost <= 0)
	{
		return ESOTMLightningThrowPurchaseResult::InvalidCost;
	}
	if (AvailableCoins < UnlockCost)
	{
		return ESOTMLightningThrowPurchaseResult::NotEnoughCoins;
	}

	UObject* Manager = nullptr;
	USaveGame* SaveObject = nullptr;
	FString SlotName;
	if (!GetMenuSaveContext(Manager, SaveObject, SlotName) || SlotName.IsEmpty())
	{
		return ESOTMLightningThrowPurchaseResult::NoActiveSave;
	}

	const int32 PreviousAvailableCoins = AvailableCoins;
	AvailableCoins -= UnlockCost;
	bLightningThrowUnlocked = true;

	if (!SavePlayerStateInternal(TEXT("LightningThrowPurchase")))
	{
		AvailableCoins = PreviousAvailableCoins;
		bLightningThrowUnlocked = false;
		return ESOTMLightningThrowPurchaseResult::SaveFailed;
	}

	OnCoinsChanged.Broadcast(AvailableCoins, LifetimeCoinsCollected);
	OnLightningThrowOwnershipChanged.Broadcast(bLightningThrowUnlocked);
	UE_LOG(LogTemp, Display,
		TEXT("SOTM Lightning Throw purchased slot=\"%s\" cost=%d available=%d lifetime=%d"),
		*SlotName, UnlockCost, AvailableCoins, LifetimeCoinsCollected);
	return ESOTMLightningThrowPurchaseResult::Success;
}

bool USOTMPlayerStateSubsystem::CommitPhase4ChestOpened()
{
	if (bPhase4ChestOpened)
	{
		return true;
	}
	bPhase4ChestOpened = true;
	if (!SavePlayerStateInternal(TEXT("Phase4ChestOpened")))
	{
		bPhase4ChestOpened = false;
		return false;
	}
	OnPhase4ProgressChanged.Broadcast(
		bPhase4ChestOpened, bPhase4HasGateKey, bPhase4GateUnlocked, bPhase4DemoCompleted);
	return true;
}

bool USOTMPlayerStateSubsystem::CommitPhase4GateKey()
{
	if (bPhase4HasGateKey)
	{
		return true;
	}
	bPhase4HasGateKey = true;
	if (!SavePlayerStateInternal(TEXT("Phase4GateKey")))
	{
		bPhase4HasGateKey = false;
		return false;
	}
	OnPhase4ProgressChanged.Broadcast(
		bPhase4ChestOpened, bPhase4HasGateKey, bPhase4GateUnlocked, bPhase4DemoCompleted);
	return true;
}

bool USOTMPlayerStateSubsystem::CommitPhase4GateUnlocked()
{
	if (bPhase4GateUnlocked)
	{
		return true;
	}
	bPhase4GateUnlocked = true;
	if (!SavePlayerStateInternal(TEXT("Phase4GateUnlocked")))
	{
		bPhase4GateUnlocked = false;
		return false;
	}
	OnPhase4ProgressChanged.Broadcast(
		bPhase4ChestOpened, bPhase4HasGateKey, bPhase4GateUnlocked, bPhase4DemoCompleted);
	return true;
}

bool USOTMPlayerStateSubsystem::CommitPhase4DemoCompleted()
{
	if (bPhase4DemoCompleted)
	{
		return true;
	}
	bPhase4DemoCompleted = true;
	if (!SavePlayerStateInternal(TEXT("Phase4DemoCompleted")))
	{
		bPhase4DemoCompleted = false;
		return false;
	}
	OnPhase4ProgressChanged.Broadcast(
		bPhase4ChestOpened, bPhase4HasGateKey, bPhase4GateUnlocked, bPhase4DemoCompleted);
	return true;
}

bool USOTMPlayerStateSubsystem::TryUnlockSkillUpgrade(
	const FName AbilityId, const int32 TargetLevel, const int32 AbilityPointCost)
{
	if (TargetLevel < 2)
	{
		return false;
	}
	const int32 CurrentUpgradeLevel = SkillUpgradeLevels.FindRef(AbilityId);
	const int32 RequiredPreviousUpgradeLevel = TargetLevel - 2;
	if (CurrentUpgradeLevel != RequiredPreviousUpgradeLevel)
	{
		// Either already bought (or beyond), or trying to skip a level.
		return false;
	}
	if (AbilityPoints < AbilityPointCost)
	{
		return false;
	}
	const int32 PreviousAbilityPoints = AbilityPoints;
	const int32 PreviousUpgradeLevel = CurrentUpgradeLevel;
	AbilityPoints -= AbilityPointCost;
	SkillUpgradeLevels.Add(AbilityId, TargetLevel - 1);

	// Persist the complete transaction before confirming it, matching every other
	// ability purchase. If the production slot cannot save, roll back so a failed
	// click never leaves points spent without the upgrade (or vice versa).
	if (!SavePlayerStateInternal(TEXT("SkillTreeUpgrade")))
	{
		AbilityPoints = PreviousAbilityPoints;
		SkillUpgradeLevels.Add(AbilityId, PreviousUpgradeLevel);
		return false;
	}
	return true;
}

void USOTMPlayerStateSubsystem::GrantAbilityPointsForTesting(const int32 Amount)
{
	const int32 PreviousAbilityPoints = AbilityPoints;
	AbilityPoints = FMath::Max(0, AbilityPoints + Amount);
	if (!SavePlayerStateInternal(TEXT("GrantAbilityPointsForTesting")))
	{
		AbilityPoints = PreviousAbilityPoints;
		UE_LOG(LogTemp, Warning, TEXT("SOTM.GrantAbilityPoints: save failed, points not granted."));
	}
}

void USOTMPlayerStateSubsystem::HandleGrantAbilityPointsCommand(const TArray<FString>& Args)
{
	if (Args.Num() < 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("Usage: SOTM.GrantAbilityPoints <amount>"));
		return;
	}
	const int32 Amount = FCString::Atoi(*Args[0]);
	GrantAbilityPointsForTesting(Amount);
	UE_LOG(LogTemp, Display, TEXT("SOTM.GrantAbilityPoints: granted %d, now have %d (saved)."), Amount, AbilityPoints);
}

void USOTMPlayerStateSubsystem::HandleGiveIsabelGateKeyCommand(const TArray<FString>& Args)
{
	// The Mansion Gate unlocks with the same key the Phase 4 chest grants, so this
	// testing command now just grants that shared key rather than a separate one.
	(void)Args;
	CommitPhase4GateKey();
	UE_LOG(LogTemp, Display, TEXT("SOTM.GiveIsabelGateKey: granted shared gate key (HasKey=%d, saved)."), bPhase4HasGateKey);
}

void USOTMPlayerStateSubsystem::HandleResetIsabelGateCommand(const TArray<FString>& Args)
{
	// Testing convenience: the gate's own state is normally one-way (Commit* only
	// ever sets flags true), so once you've unlocked it in a save there is no way
	// back to the closed/prompt-visible state without this. Does NOT touch the
	// shared Phase4 gate key (bPhase4HasGateKey) - only the gate's own progress.
	(void)Args;
	bIsabelGateReached = false;
	bHasIsabelGateKey = false;
	bIsabelGateUnlocked = false;
	SavePlayerStateInternal(TEXT("IsabelGateReset"));
	OnIsabelGateProgressChanged.Broadcast(bIsabelGateReached, bHasIsabelGateKey, bIsabelGateUnlocked);
	UE_LOG(LogTemp, Display, TEXT("SOTM.ResetIsabelGate: cleared reached/key/unlocked and saved."));
}

void USOTMPlayerStateSubsystem::HandleGoToIsabelArenaCommand(const TArray<FString>& Args)
{
	// Testing convenience only - finds the level actor tagged "IsabelArena" (add
	// that tag to BP_IsabelArena's Actor > Tags in the level) and teleports the
	// player pawn to its Box component, so you don't have to walk the level each
	// time to test the boss fight.
	(void)Args;

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOTM.GoToIsabelArena: no World available."));
		return;
	}

	static const FName IsabelArenaTag(TEXT("IsabelArena"));
	TArray<AActor*> ArenaActors;
	UGameplayStatics::GetAllActorsWithTag(World, IsabelArenaTag, ArenaActors);
	if (ArenaActors.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOTM.GoToIsabelArena: no actor tagged 'IsabelArena' found - add that tag to BP_IsabelArena's Actor > Tags in the level."));
		return;
	}

	const AActor* Arena = ArenaActors[0];
	FVector TargetLocation = Arena->GetActorLocation();
	if (const UBoxComponent* ArenaBox = Arena->FindComponentByClass<UBoxComponent>())
	{
		TargetLocation = ArenaBox->GetComponentLocation();
	}
	TargetLocation.Z += 100.0f;

	AActor* PlayerActor = BoundPlayerActor.Get();
	if (!PlayerActor)
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			PlayerActor = PC->GetPawn();
		}
	}
	if (!PlayerActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOTM.GoToIsabelArena: no player pawn found."));
		return;
	}

	PlayerActor->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
	UE_LOG(LogTemp, Display, TEXT("SOTM.GoToIsabelArena: teleported player to (%s)."), *TargetLocation.ToString());
}

bool USOTMPlayerStateSubsystem::CommitIsabelGateReached()
{
	if (bIsabelGateReached)
	{
		return true;
	}
	bIsabelGateReached = true;
	if (!SavePlayerStateInternal(TEXT("IsabelGateReached")))
	{
		bIsabelGateReached = false;
		return false;
	}
	OnIsabelGateProgressChanged.Broadcast(bIsabelGateReached, bHasIsabelGateKey, bIsabelGateUnlocked);
	return true;
}

bool USOTMPlayerStateSubsystem::CommitIsabelGateKey()
{
	if (bHasIsabelGateKey)
	{
		return true;
	}
	bHasIsabelGateKey = true;
	if (!SavePlayerStateInternal(TEXT("IsabelGateKey")))
	{
		bHasIsabelGateKey = false;
		return false;
	}
	OnIsabelGateProgressChanged.Broadcast(bIsabelGateReached, bHasIsabelGateKey, bIsabelGateUnlocked);
	return true;
}

bool USOTMPlayerStateSubsystem::CommitIsabelGateUnlocked()
{
	if (bIsabelGateUnlocked)
	{
		return true;
	}
	bIsabelGateUnlocked = true;
	if (!SavePlayerStateInternal(TEXT("IsabelGateUnlocked")))
	{
		bIsabelGateUnlocked = false;
		return false;
	}
	OnIsabelGateProgressChanged.Broadcast(bIsabelGateReached, bHasIsabelGateKey, bIsabelGateUnlocked);
	return true;
}

float USOTMPlayerStateSubsystem::GetEffectiveSpeedBoostDuration() const
{
	float Value = GetDefault<USOTMPhase3Settings>()->SpeedBoostDuration;
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const int32 UpgradeLevel = GetSkillUpgradeLevel(TEXT("SpeedBoost"));
	for (int32 Level = 2; Level <= 1 + UpgradeLevel; ++Level)
	{
		if (const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(TEXT("SpeedBoost"), Level) : nullptr)
		{
			Value += LevelDef->DurationDeltaSeconds;
		}
	}
	return Value;
}

float USOTMPlayerStateSubsystem::GetEffectiveSpeedBoostCooldown() const
{
	float Value = GetDefault<USOTMPhase3Settings>()->SpeedBoostCooldown;
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const int32 UpgradeLevel = GetSkillUpgradeLevel(TEXT("SpeedBoost"));
	for (int32 Level = 2; Level <= 1 + UpgradeLevel; ++Level)
	{
		if (const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(TEXT("SpeedBoost"), Level) : nullptr)
		{
			Value += LevelDef->CooldownDeltaSeconds;
		}
	}
	return FMath::Max(0.1f, Value);
}

float USOTMPlayerStateSubsystem::GetEffectiveLightningThrowCooldown() const
{
	float Value = GetDefault<USOTMLightningThrowSettings>()->LightningThrowCooldown;
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const int32 UpgradeLevel = GetSkillUpgradeLevel(TEXT("LightningThrow"));
	for (int32 Level = 2; Level <= 1 + UpgradeLevel; ++Level)
	{
		if (const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(TEXT("LightningThrow"), Level) : nullptr)
		{
			Value += LevelDef->CooldownDeltaSeconds;
		}
	}
	return FMath::Max(0.1f, Value);
}

float USOTMPlayerStateSubsystem::GetEffectiveLightningThrowRange() const
{
	float Value = GetDefault<USOTMLightningThrowSettings>()->LightningThrowRange;
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const int32 UpgradeLevel = GetSkillUpgradeLevel(TEXT("LightningThrow"));
	for (int32 Level = 2; Level <= 1 + UpgradeLevel; ++Level)
	{
		if (const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(TEXT("LightningThrow"), Level) : nullptr)
		{
			Value += LevelDef->RangeDelta;
		}
	}
	return Value;
}

bool USOTMPlayerStateSubsystem::IsCoinCollected(const FGuid PersistentCoinId) const
{
	return PersistentCoinId.IsValid() && CollectedCoinIds.Contains(PersistentCoinId);
}

bool USOTMPlayerStateSubsystem::IsBoundPlayerActor(const AActor* Actor) const
{
	return IsValid(Actor) && Actor == BoundPlayerActor.Get();
}

bool USOTMPlayerStateSubsystem::ActivateCheckpoint(
	const FName CheckpointId,
	const FName MapPackageName,
	const FTransform& RespawnTransform,
	const bool bSaveImmediately)
{
	if (CheckpointId.IsNone() || MapPackageName.IsNone())
	{
		return false;
	}

	if (CheckpointState.bIsValid &&
		CheckpointState.CheckpointId == CheckpointId &&
		CheckpointState.MapPackageName == MapPackageName)
	{
		return false;
	}

	CheckpointState.bIsValid = true;
	CheckpointState.CheckpointId = CheckpointId;
	CheckpointState.MapPackageName = MapPackageName;
	CheckpointState.RespawnTransform = RespawnTransform;
	OnCheckpointChanged.Broadcast(CheckpointId, MapPackageName, RespawnTransform);

	if (bSaveImmediately && GetDefault<USOTMPlayerSystemSettings>()->bAutoSaveOnCheckpoint)
	{
		SavePlayerState();
	}

	return true;
}

bool USOTMPlayerStateSubsystem::ForceRespawnAtCheckpoint()
{
	if (!CheckpointState.bIsValid || !BoundPlayerActor || !BoundVitalComponent)
	{
		return false;
	}

	if (CheckpointState.MapPackageName != CurrentGameplayMap)
	{
		bPendingRespawnAfterTravel = true;
		UGameplayStatics::OpenLevel(this, CheckpointState.MapPackageName);
		return true;
	}

	PerformRespawn();
	return true;
}

bool USOTMPlayerStateSubsystem::RetryFromGameOver()
{
	UE_LOG(LogTemp, Display, TEXT("SOTM Game Over: RetryFromGameOver called (bGameOver=%d)."), bGameOver);
	if (!bGameOver)
	{
		return false;
	}

	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	if (Settings->RetryPolicy == ESOTMGameOverRetryPolicy::Disabled)
	{
		UE_LOG(LogTemp, Warning, TEXT("SOTM Game Over: RetryFromGameOver no-op - RetryPolicy is Disabled in Project Settings > Game > SOTM Player System > Game Over."));
		return false;
	}

	HideGameOverWidget();
	bGameOver = false;
	CurrentLives = Settings->RetryPolicy == ESOTMGameOverRetryPolicy::RestoreOneLifeAtCheckpoint
		? 1
		: MaximumLives;
	OnLivesChanged.Broadcast(CurrentLives, MaximumLives);
	ClearInputLock(ESOTMInputLockReason::GameOver);
	return ForceRespawnAtCheckpoint();
}

void USOTMPlayerStateSubsystem::ReturnToMainMenu()
{
	UE_LOG(LogTemp, Display, TEXT("SOTM Game Over: ReturnToMainMenu called, opening %s."),
		*GetDefault<USOTMPlayerSystemSettings>()->MainMenuMap.ToString());
	HideGameOverWidget();
	bGameOver = false;
	bPlayerDead = false;
	bDeathProcessing = false;
	ClearAllInputLocks();

	// AddToViewport widgets (the Game Over screen just hidden above, but also the
	// in-game HUD and any other UI left over from gameplay) are owned by the
	// UGameViewportClient, which is NOT destroyed by OpenLevel() - only the World and
	// its actors are. Without this, leftover UI stays rendered on top of the newly
	// loaded main menu level even though HideGameOverWidget() already removed the Game
	// Over screen specifically.
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->RemoveAllViewportWidgets();
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::OpenLevel(World, GetDefault<USOTMPlayerSystemSettings>()->MainMenuMap);
	}
}

void USOTMPlayerStateSubsystem::AcquireInputLock(const ESOTMInputLockReason Reason)
{
	int32& Count = InputLockCounts.FindOrAdd(Reason);
	++Count;
	ApplyInputLockState();
}

void USOTMPlayerStateSubsystem::ReleaseInputLock(const ESOTMInputLockReason Reason)
{
	if (int32* Count = InputLockCounts.Find(Reason))
	{
		*Count = FMath::Max(0, *Count - 1);
		if (*Count == 0)
		{
			InputLockCounts.Remove(Reason);
		}
	}
	ApplyInputLockState();
}

void USOTMPlayerStateSubsystem::ClearInputLock(const ESOTMInputLockReason Reason)
{
	InputLockCounts.Remove(Reason);
	ApplyInputLockState();
}

void USOTMPlayerStateSubsystem::ClearAllInputLocks()
{
	InputLockCounts.Reset();
	ApplyInputLockState();
}

bool USOTMPlayerStateSubsystem::HasAnyInputLock() const
{
	return InputLockCounts.Num() > 0;
}

int32 USOTMPlayerStateSubsystem::GetInputLockCount(const ESOTMInputLockReason Reason) const
{
	return InputLockCounts.FindRef(Reason);
}

TArray<ESOTMInputLockReason> USOTMPlayerStateSubsystem::GetActiveInputLockReasons() const
{
	TArray<ESOTMInputLockReason> Reasons;
	InputLockCounts.GetKeys(Reasons);
	return Reasons;
}

bool USOTMPlayerStateSubsystem::SavePlayerState()
{
	return SavePlayerStateInternal(TEXT("Explicit"));
}

bool USOTMPlayerStateSubsystem::SavePlayerStateInternal(const TCHAR* Reason)
{
	UObject* Manager = nullptr;
	USaveGame* SaveObject = nullptr;
	FString SlotName;
	if (!GetMenuSaveContext(Manager, SaveObject, SlotName) || !WriteStateToSaveObject(SaveObject))
	{
		UE_LOG(LogTemp, Warning, TEXT("SOTM Player State: menu SaveGame object is not ready; player state was not saved."));
		return false;
	}

	bool bSaved = false;
	if (UFunction* SaveFunction = Manager->FindFunction(TEXT("SaveGame")))
	{
		struct FSaveGameParameters
		{
			FString SlotName;
			int32 UserIndex = 0;
			bool bSuccess = false;
		};

		FSaveGameParameters Parameters;
		Parameters.SlotName = SlotName;
		Manager->ProcessEvent(SaveFunction, &Parameters);
		bSaved = Parameters.bSuccess;
	}
	else
	{
		// Compatibility fallback for a replacement manager that exposes the same
		// reflected state but no SaveGame function.
		bSaved = UGameplayStatics::SaveGameToSlot(SaveObject, SlotName, 0);
	}

	if (bSaved)
	{
		bCoinStateDirty = false;
	}

#if !UE_BUILD_SHIPPING
	const FString SaveFilename = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SlotName + TEXT(".sav"));
	const FDateTime Timestamp = IFileManager::Get().GetTimeStamp(*SaveFilename);
	UE_LOG(
		LogTemp,
		Display,
		TEXT("[CoinSave] WRITE success=%d slot=\"%s\" user=0 class=%s available=%d lifetime=%d ids=%d reason=%s timestamp=%s"),
		bSaved,
		*SlotName,
		*SaveObject->GetClass()->GetPathName(),
		AvailableCoins,
		LifetimeCoinsCollected,
		CollectedCoinIds.Num(),
		Reason,
		*Timestamp.ToIso8601());
#endif
	return bSaved;
}

bool USOTMPlayerStateSubsystem::LoadPlayerState(const bool bTravelToCheckpoint)
{
	UObject* Manager = nullptr;
	USaveGame* SaveObject = nullptr;
	FString SlotName;
	if (!GetMenuSaveContext(Manager, SaveObject, SlotName))
	{
		return false;
	}

	if (!ReadStateFromSaveObject(SaveObject))
	{
		return false;
	}

	bLoadedPlayerState = true;
	LastSynchronizedSaveSlot = SlotName;
	LastSynchronizedSaveObject = SaveObject;
	ApplyLoadedStateToBoundPlayer();

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display,
		TEXT("[CoinSave] READ slot=\"%s\" user=0 class=%s available=%d lifetime=%d ids=%d reason=ExplicitLoad"),
		*SlotName, *SaveObject->GetClass()->GetPathName(), AvailableCoins,
		LifetimeCoinsCollected, CollectedCoinIds.Num());
#endif

	if (bTravelToCheckpoint &&
		CheckpointState.bIsValid &&
		!CurrentGameplayMap.IsNone() &&
		CheckpointState.MapPackageName != CurrentGameplayMap)
	{
		bPendingRespawnAfterTravel = true;
		UGameplayStatics::OpenLevel(this, CheckpointState.MapPackageName);
	}

	return true;
}

bool USOTMPlayerStateSubsystem::SavePlayerStateToSlot(const FString& SlotName)
{
	FString CleanSlotName = SlotName;
	CleanSlotName.TrimStartAndEndInline();
	if (CleanSlotName.IsEmpty())
	{
		return false;
	}

	USaveGame* SaveObject = nullptr;
	if (UGameplayStatics::DoesSaveGameExist(CleanSlotName, 0))
	{
		SaveObject = UGameplayStatics::LoadGameFromSlot(CleanSlotName, 0);
	}

	if (!SaveObject)
	{
		UClass* SaveClass = LoadClass<USaveGame>(
			nullptr,
			TEXT("/Game/MenuSystemPro/Blueprints/SaveGame/BP_CustomSaveGameObject.BP_CustomSaveGameObject_C"));
		if (!SaveClass)
		{
			return false;
		}
		SaveObject = UGameplayStatics::CreateSaveGameObject(SaveClass);
	}

	const bool bSaved = WriteStateToSaveObject(SaveObject) &&
		UGameplayStatics::SaveGameToSlot(SaveObject, CleanSlotName, 0);
	if (bSaved)
	{
		bCoinStateDirty = false;
	}
	return bSaved;
}

bool USOTMPlayerStateSubsystem::LoadPlayerStateFromSlot(
	const FString& SlotName,
	const bool bTravelToCheckpoint)
{
	FString CleanSlotName = SlotName;
	CleanSlotName.TrimStartAndEndInline();
	if (CleanSlotName.IsEmpty() || !UGameplayStatics::DoesSaveGameExist(CleanSlotName, 0))
	{
		return false;
	}

	USaveGame* SaveObject = UGameplayStatics::LoadGameFromSlot(CleanSlotName, 0);
	if (!SaveObject || !ReadStateFromSaveObject(SaveObject))
	{
		return false;
	}

	bLoadedPlayerState = true;
	ApplyLoadedStateToBoundPlayer();

	if (bTravelToCheckpoint &&
		CheckpointState.bIsValid &&
		!CurrentGameplayMap.IsNone() &&
		CheckpointState.MapPackageName != CurrentGameplayMap)
	{
		bPendingRespawnAfterTravel = true;
		UGameplayStatics::OpenLevel(this, CheckpointState.MapPackageName);
	}

	return true;
}

void USOTMPlayerStateSubsystem::ResetRuntimeStateForNewGame()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandle);
		World->GetTimerManager().ClearTimer(GameOverTimerHandle);
	}

	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	MaximumLives = FMath::Max(1, Settings->MaximumLives);
	CurrentLives = FMath::Clamp(Settings->StartingLives, 1, MaximumLives);
	PersistentHealth = FMath::Max(1.0f, Settings->MaximumHealth);
	AvailableCoins = 0;
	LifetimeCoinsCollected = 0;
	bSpeedBoostUnlocked = false;
	SpeedBoostLevel = 0;
	bLightningThrowUnlocked = false;
	bPhase4ChestOpened = false;
	bPhase4HasGateKey = false;
	bPhase4GateUnlocked = false;
	bPhase4DemoCompleted = false;
	CollectedCoinIds.Reset();
	bCoinStateDirty = false;
	bPlayerDead = false;
	bGameOver = false;
	bDeathProcessing = false;
	bLoadedPlayerState = false;
	bPendingMansionIntro = false;
	bHasInitializedPlayerHealth = false;
	bPendingRespawnAfterTravel = false;
	CheckpointState = FSOTMCheckpointState();
	HideGameOverWidget();
	ClearAllInputLocks();
	ApplyLoadedStateToBoundPlayer();
	OnLivesChanged.Broadcast(CurrentLives, MaximumLives);
	OnCoinsChanged.Broadcast(AvailableCoins, LifetimeCoinsCollected);
	OnSpeedBoostOwnershipChanged.Broadcast(bSpeedBoostUnlocked, SpeedBoostLevel);
	OnLightningThrowOwnershipChanged.Broadcast(bLightningThrowUnlocked);
	OnPhase4ProgressChanged.Broadcast(false, false, false, false);
}

bool USOTMPlayerStateSubsystem::ConsumePendingMansionIntro()
{
	const bool bWasPending = bPendingMansionIntro;
	bPendingMansionIntro = false;
	return bWasPending;
}

bool USOTMPlayerStateSubsystem::SynchronizeFromActiveMenuSave(
	const TCHAR* Reason,
	const bool bResetIfSaveHasNoPlayerState)
{
	UObject* Manager = nullptr;
	USaveGame* SaveObject = nullptr;
	FString SlotName;
	if (!GetMenuSaveContext(Manager, SaveObject, SlotName))
	{
		return false;
	}

	if (LastSynchronizedSaveObject.Get() == SaveObject && LastSynchronizedSaveSlot == SlotName)
	{
		return bLoadedPlayerState;
	}

	bAutoLoadAttempted = true;
	const bool bRead = ReadStateFromSaveObject(SaveObject);
	if (bRead)
	{
		bPendingMansionIntro = false;
		bLoadedPlayerState = true;
		ApplyLoadedStateToBoundPlayer();
	}
	else if (bResetIfSaveHasNoPlayerState)
	{
		// A newly-created production slot has no SOTM schema yet. This is the
		// authoritative New Game signal; reset here, never during generic startup.
		ResetRuntimeStateForNewGame();
		bPendingMansionIntro = true;
	}

	LastSynchronizedSaveSlot = SlotName;
	LastSynchronizedSaveObject = SaveObject;

#if !UE_BUILD_SHIPPING
	UE_LOG(LogTemp, Display,
		TEXT("[CoinSave] READ success=%d slot=\"%s\" user=0 class=%s available=%d lifetime=%d ids=%d reason=%s%s"),
		bRead,
		*SlotName,
		*SaveObject->GetClass()->GetPathName(),
		AvailableCoins,
		LifetimeCoinsCollected,
		CollectedCoinIds.Num(),
		Reason,
		(!bRead && bResetIfSaveHasNoPlayerState) ? TEXT(" (NewGameReset)") : TEXT(""));
#endif
	return bRead;
}

void USOTMPlayerStateSubsystem::SetLivesForDebug(const int32 NewLives)
{
	CurrentLives = FMath::Clamp(NewLives, 0, MaximumLives);
	OnLivesChanged.Broadcast(CurrentLives, MaximumLives);
}

void USOTMPlayerStateSubsystem::SetPhase3ProgressForDebug(
	const int32 NewAvailableCoins,
	const int32 NewLifetimeCoins,
	const bool bUnlockSpeedBoost,
	const int32 NewSpeedBoostLevel)
{
#if UE_BUILD_SHIPPING
	(void)NewAvailableCoins;
	(void)NewLifetimeCoins;
	(void)bUnlockSpeedBoost;
	(void)NewSpeedBoostLevel;
#else
	AvailableCoins = FMath::Max(0, NewAvailableCoins);
	LifetimeCoinsCollected = FMath::Max(AvailableCoins, NewLifetimeCoins);
	bSpeedBoostUnlocked = bUnlockSpeedBoost && NewSpeedBoostLevel > 0;
	SpeedBoostLevel = bSpeedBoostUnlocked ? FMath::Max(1, NewSpeedBoostLevel) : 0;
	OnCoinsChanged.Broadcast(AvailableCoins, LifetimeCoinsCollected);
	OnSpeedBoostOwnershipChanged.Broadcast(bSpeedBoostUnlocked, SpeedBoostLevel);
#endif
}

void USOTMPlayerStateSubsystem::SetPhase4ProgressForDebug(
	const bool bChestOpened,
	const bool bHasGateKey,
	const bool bGateUnlocked,
	const bool bDemoCompleted)
{
#if UE_BUILD_SHIPPING
	(void)bChestOpened;
	(void)bHasGateKey;
	(void)bGateUnlocked;
	(void)bDemoCompleted;
#else
	bPhase4ChestOpened = bChestOpened;
	bPhase4HasGateKey = bHasGateKey || bChestOpened;
	bPhase4GateUnlocked = bGateUnlocked;
	bPhase4DemoCompleted = bDemoCompleted;
	OnPhase4ProgressChanged.Broadcast(
		bPhase4ChestOpened, bPhase4HasGateKey, bPhase4GateUnlocked, bPhase4DemoCompleted);
#endif
}

void USOTMPlayerStateSubsystem::HandleHealthChanged(
	USOTMPlayerVitalComponent* VitalComponent,
	const float PreviousHealth,
	const float CurrentHealth,
	const float InMaximumHealth)
{
	PersistentHealth = CurrentHealth;
	SyncLegacyCharacterState();
}

void USOTMPlayerStateSubsystem::HandlePlayerDeath(
	USOTMPlayerVitalComponent* VitalComponent,
	AController* InstigatedBy,
	AActor* DamageCauser)
{
	if (bDeathProcessing || bGameOver)
	{
		return;
	}

	bDeathProcessing = true;
	bPlayerDead = true;
	PersistentHealth = 0.0f;
	AcquireInputLock(ESOTMInputLockReason::Death);

	if (ACharacter* Character = Cast<ACharacter>(BoundPlayerActor))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->DisableMovement();
		}
	}

	CurrentLives = FMath::Max(0, CurrentLives - 1);
	OnLivesChanged.Broadcast(CurrentLives, MaximumLives);
	OnPlayerDeathStarted.Broadcast(BoundPlayerActor);
	SyncLegacyCharacterState();

	if (GetDefault<USOTMPlayerSystemSettings>()->bAutoSaveOnDeath)
	{
		SavePlayerState();
	}

	if (CurrentLives <= 0)
	{
		ScheduleGameOver();
	}
	else
	{
		ScheduleRespawn();
	}
}

void USOTMPlayerStateSubsystem::ScheduleRespawn()
{
	if (UWorld* World = GetWorld())
	{
		const float Delay = FMath::Max(0.0f, GetDefault<USOTMPlayerSystemSettings>()->RespawnDelaySeconds);
		World->GetTimerManager().SetTimer(
			RespawnTimerHandle,
			this,
			&USOTMPlayerStateSubsystem::PerformRespawn,
			FMath::Max(0.01f, Delay),
			false);
	}
}

void USOTMPlayerStateSubsystem::PerformRespawn()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GameOverTimerHandle);
	}

	if (!CheckpointState.bIsValid || !BoundPlayerActor || !BoundVitalComponent)
	{
		return;
	}

	if (CheckpointState.MapPackageName != CurrentGameplayMap)
	{
		bPendingRespawnAfterTravel = true;
		UGameplayStatics::OpenLevel(this, CheckpointState.MapPackageName);
		return;
	}

	AcquireInputLock(ESOTMInputLockReason::Respawn);
	BoundPlayerActor->SetActorTransform(
		CheckpointState.RespawnTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (ACharacter* Character = Cast<ACharacter>(BoundPlayerActor))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
			Movement->SetMovementMode(MOVE_Walking);
		}
	}

	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	const float RespawnHealth = Settings->bRespawnWithFullHealth
		? BoundVitalComponent->GetMaximumHealth()
		: BoundVitalComponent->GetMaximumHealth() * FMath::Clamp(Settings->RespawnHealthFraction, 0.01f, 1.0f);
	BoundVitalComponent->Revive(RespawnHealth);
	BoundVitalComponent->SetInvulnerableForDuration(Settings->RespawnSafetyInvulnerabilitySeconds);
	PersistentHealth = RespawnHealth;
	bPlayerDead = false;
	bDeathProcessing = false;

	// Continuing from a Game Over save (LoadPlayerStateFromSlot -> checkpoint travel)
	// runs this same successful-respawn path with CurrentLives still at the 0 the save
	// was written with, and with bGameOver left set by ApplyLoadedStateToBoundPlayer's
	// resume-Game-Over branch. A respawn that actually reaches this point means the
	// player is back in the world and playable, so any lingering Game Over UI/state
	// must be cleared here too - mirroring RetryFromGameOver's own cleanup - or the
	// widget is left stuck on screen over an otherwise-working game.
	if (bGameOver)
	{
		HideGameOverWidget();
		bGameOver = false;
		CurrentLives = Settings->RetryPolicy == ESOTMGameOverRetryPolicy::RestoreOneLifeAtCheckpoint
			? 1
			: MaximumLives;
		OnLivesChanged.Broadcast(CurrentLives, MaximumLives);
		ClearInputLock(ESOTMInputLockReason::GameOver);
	}

	ClearInputLock(ESOTMInputLockReason::Death);
	ClearInputLock(ESOTMInputLockReason::Respawn);
	SyncLegacyCharacterState();
	OnPlayerRespawned.Broadcast(BoundPlayerActor);
	SavePlayerState();
}

void USOTMPlayerStateSubsystem::ScheduleGameOver()
{
	if (UWorld* World = GetWorld())
	{
		const float PresentationSeconds = FMath::Max(0.01f,
			GetDefault<USOTMPlayerSystemSettings>()->RespawnDelaySeconds);
		World->GetTimerManager().SetTimer(
			GameOverTimerHandle,
			this,
			&USOTMPlayerStateSubsystem::TriggerGameOver,
			PresentationSeconds,
			false);
		UE_LOG(LogTemp, Display,
			TEXT("SOTM Player State: final-life Game Over UI delayed %.2fs for death presentation."),
			PresentationSeconds);
	}
}

void USOTMPlayerStateSubsystem::TriggerGameOver()
{
	if (bGameOver)
	{
		return;
	}

	bGameOver = true;
	AcquireInputLock(ESOTMInputLockReason::GameOver);
	ShowGameOverWidget();
	OnGameOver.Broadcast();
}

void USOTMPlayerStateSubsystem::ShowGameOverWidget()
{
	if (GameOverWidget || !GetWorld())
	{
		return;
	}

	APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	if (!PlayerController)
	{
		return;
	}

	// UI is built in the Designer (WBP_GameOver, a Blueprint subclass of
	// USOTMGameOverWidget) instead of the old hand-built Slate widget, so load that
	// class if one is configured - same pattern as
	// USOTMDemoPhase3WorldSubsystem::OpenSkillTreeUI reading SkillTreeWidgetClass.
	TSubclassOf<USOTMGameOverWidget> GameOverWidgetClass =
		GetDefault<USOTMPlayerSystemSettings>()->GameOverWidgetClass;
	if (!GameOverWidgetClass)
	{
		GameOverWidgetClass = USOTMGameOverWidget::StaticClass();
	}
	GameOverWidget = CreateWidget<USOTMGameOverWidget>(PlayerController, GameOverWidgetClass);
	if (GameOverWidget)
	{
		GameOverWidget->AddToViewport(10000);

		// SetWidgetToFocus matters here, not just cosmetically: without it, Slate keeps
		// whatever had focus before (usually the game viewport), and the FIRST click on
		// any button in the new widget can get consumed just shifting focus onto this
		// widget rather than actually firing that button's OnClicked - which looks
		// exactly like "I clicked Main Menu and nothing happened." Explicitly focusing
		// this widget's Slate root avoids that.
		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(GameOverWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetShowMouseCursor(true);
		PlayerController->SetInputMode(InputMode);
		UE_LOG(LogTemp, Display, TEXT("SOTM Game Over: widget shown (class=%s), mouse cursor + UI-only input mode applied."),
			*GameOverWidgetClass->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SOTM Game Over: CreateWidget<USOTMGameOverWidget> failed (class=%s)."),
			*GameOverWidgetClass->GetName());
	}
}

void USOTMPlayerStateSubsystem::HideGameOverWidget()
{
	if (GameOverWidget)
	{
		GameOverWidget->RemoveFromParent();
		GameOverWidget = nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			FInputModeGameOnly InputMode;
			PlayerController->SetInputMode(InputMode);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

void USOTMPlayerStateSubsystem::ApplyInputLockState()
{
	APlayerController* CurrentController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;

	if (bInputBlockApplied && InputLockedController.IsValid() && InputLockedController.Get() != CurrentController)
	{
		InputLockedController->SetIgnoreMoveInput(false);
		InputLockedController->SetIgnoreLookInput(false);
		bInputBlockApplied = false;
	}

	const bool bShouldBlock = HasAnyInputLock() && CurrentController;
	if (bShouldBlock && !bInputBlockApplied)
	{
		CurrentController->SetIgnoreMoveInput(true);
		CurrentController->SetIgnoreLookInput(true);
		InputLockedController = CurrentController;
		bInputBlockApplied = true;
	}
	else if (!bShouldBlock && bInputBlockApplied && InputLockedController.IsValid())
	{
		InputLockedController->SetIgnoreMoveInput(false);
		InputLockedController->SetIgnoreLookInput(false);
		InputLockedController.Reset();
		bInputBlockApplied = false;
	}

	OnInputLocksChanged.Broadcast(HasAnyInputLock(), GetActiveInputLockReasons());
}

void USOTMPlayerStateSubsystem::SyncLegacyCharacterState()
{
	if (!BoundPlayerActor)
	{
		return;
	}

	SOTMPlayerStatePrivate::SetFloatProperty(BoundPlayerActor, TEXT("HP"), PersistentHealth);
	SOTMPlayerStatePrivate::SetFloatProperty(BoundPlayerActor, TEXT("CurrentHealth"), PersistentHealth);
	SOTMPlayerStatePrivate::SetBoolProperty(BoundPlayerActor, TEXT("isdead?"), bPlayerDead);
}

void USOTMPlayerStateSubsystem::EnsureAutomaticMapEntryCheckpoint()
{
	if (!BoundPlayerActor || CurrentGameplayMap.IsNone())
	{
		return;
	}

	if (!CheckpointState.bIsValid || CheckpointState.MapPackageName != CurrentGameplayMap)
	{
		const FString ShortMapName = FPackageName::GetShortName(CurrentGameplayMap.ToString());
		const FName AutomaticId(*FString::Printf(TEXT("MapEntry_%s"), *ShortMapName));
		ActivateCheckpoint(AutomaticId, CurrentGameplayMap, BoundPlayerActor->GetActorTransform(), false);
	}
}

void USOTMPlayerStateSubsystem::ApplyLoadedStateToBoundPlayer()
{
	if (!BoundVitalComponent)
	{
		return;
	}

	const float MaximumHealth = FMath::Max(1.0f, GetDefault<USOTMPlayerSystemSettings>()->MaximumHealth);
	const bool bUsePersistentHealth = bLoadedPlayerState || bHasInitializedPlayerHealth;
	const float HealthToApply = bUsePersistentHealth && PersistentHealth > 0.0f
		? FMath::Clamp(PersistentHealth, 1.0f, MaximumHealth)
		: MaximumHealth;

	BoundVitalComponent->InitializeVitals(MaximumHealth, HealthToApply);
	PersistentHealth = HealthToApply;
	bHasInitializedPlayerHealth = true;

	// The Main Menu's own preview/placeholder pawn binds through this exact same
	// path (see BindPlayer). A save with CurrentLives<=0 - which is exactly the
	// state left behind by the death that sent the player to the Main Menu in the
	// first place - must not re-trigger Game Over on top of the Main Menu itself.
	// Only a real gameplay level binding a genuinely-dead player should resume it.
	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	const bool bIsMainMenuMap = CurrentGameplayMap == Settings->MainMenuMap;
	if (!bIsMainMenuMap)
	{
		// A dead (CurrentLives<=0) save can be loaded two different ways: through the
		// Main Menu's Continue button (which travels here and then calls
		// PerformRespawn), or by simply entering/relaunching straight into a gameplay
		// level whose auto-loaded save happens to be dead (PrepareForGameplayWorld's
		// GameplayWorld sync - no travel, no PerformRespawn ever runs). Both cases land
		// the player on a map with a valid checkpoint they can actually continue from,
		// so both must be treated as a continue, not a fresh Game Over - otherwise the
		// Game Over screen is shown (or left showing) over an already-playable game.
		const bool bCanContinueHere =
			Settings->RetryPolicy != ESOTMGameOverRetryPolicy::Disabled &&
			CheckpointState.bIsValid &&
			CheckpointState.MapPackageName == CurrentGameplayMap;

		if (CurrentLives <= 0 && !bCanContinueHere)
		{
			bGameOver = false;
			bPlayerDead = true;
			bDeathProcessing = true;
			BoundVitalComponent->SetInvulnerable(true);
			TriggerGameOver();
		}
		else
		{
			if (CurrentLives <= 0)
			{
				// Same lives restoration RetryFromGameOver applies, so a save that was
				// written at 0 lives comes back playable instead of stuck on Game Over.
				CurrentLives = Settings->RetryPolicy == ESOTMGameOverRetryPolicy::RestoreOneLifeAtCheckpoint
					? 1
					: MaximumLives;
				OnLivesChanged.Broadcast(CurrentLives, MaximumLives);
				bGameOver = false;
				HideGameOverWidget();
				ClearInputLock(ESOTMInputLockReason::GameOver);
			}
			bPlayerDead = false;
			bDeathProcessing = false;
		}
	}
	SyncLegacyCharacterState();
}

bool USOTMPlayerStateSubsystem::GetMenuSaveContext(
	UObject*& OutManager,
	USaveGame*& OutSaveObject,
	FString& OutSlotName) const
{
	OutManager = nullptr;
	OutSaveObject = nullptr;
	OutSlotName = GetDefault<USOTMPlayerSystemSettings>()->FallbackSaveSlotName;

	const UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return false;
	}

	const FObjectPropertyBase* ManagerProperty =
		FindFProperty<FObjectPropertyBase>(GameInstance->GetClass(), TEXT("SaveGameManager"));
	if (!ManagerProperty)
	{
		return false;
	}

	OutManager = ManagerProperty->GetObjectPropertyValue_InContainer(GameInstance);
	if (!OutManager)
	{
		return false;
	}

	if (const FStrProperty* SlotProperty =
		FindFProperty<FStrProperty>(OutManager->GetClass(), TEXT("LastLoadedSaveSlotName")))
	{
		FString ManagerSlot = SlotProperty->GetPropertyValue_InContainer(OutManager);
		ManagerSlot.TrimStartAndEndInline();
		if (!ManagerSlot.IsEmpty())
		{
			OutSlotName = ManagerSlot;
		}
	}

	const FObjectPropertyBase* SaveObjectProperty =
		FindFProperty<FObjectPropertyBase>(OutManager->GetClass(), TEXT("SaveGameObject"));
	if (!SaveObjectProperty)
	{
		return false;
	}

	OutSaveObject = Cast<USaveGame>(SaveObjectProperty->GetObjectPropertyValue_InContainer(OutManager));
	if (!OutSaveObject && UGameplayStatics::DoesSaveGameExist(OutSlotName, 0))
	{
		OutSaveObject = UGameplayStatics::LoadGameFromSlot(OutSlotName, 0);
		if (OutSaveObject)
		{
			SaveObjectProperty->SetObjectPropertyValue_InContainer(OutManager, OutSaveObject);
		}
	}

	return OutSaveObject != nullptr;
}

bool USOTMPlayerStateSubsystem::WriteStateToSaveObject(UObject* SaveObject) const
{
	if (!SaveObject)
	{
		return false;
	}

	const bool bHasStateWritten =
		SOTMPlayerStatePrivate::SetBoolProperty(SaveObject, SOTMPlayerStatePrivate::HasStateProperty, true);
	const bool bVersionWritten =
		SOTMPlayerStatePrivate::SetIntProperty(SaveObject, SOTMPlayerStatePrivate::SaveVersionProperty, CurrentSaveVersion);
	const bool bLivesWritten =
		SOTMPlayerStatePrivate::SetIntProperty(SaveObject, SOTMPlayerStatePrivate::CurrentLivesProperty, CurrentLives);
	const bool bMaximumLivesWritten =
		SOTMPlayerStatePrivate::SetIntProperty(SaveObject, SOTMPlayerStatePrivate::MaximumLivesProperty, MaximumLives);
	const bool bHealthWritten =
		SOTMPlayerStatePrivate::SetFloatProperty(SaveObject, SOTMPlayerStatePrivate::CurrentHealthProperty, PersistentHealth);
	const bool bCheckpointIdWritten =
		SOTMPlayerStatePrivate::SetNameProperty(
			SaveObject,
			SOTMPlayerStatePrivate::CheckpointIdProperty,
			CheckpointState.CheckpointId);
	const bool bCheckpointMapWritten =
		SOTMPlayerStatePrivate::SetNameProperty(
			SaveObject,
			SOTMPlayerStatePrivate::CheckpointMapProperty,
			CheckpointState.MapPackageName);
	const bool bCheckpointTransformWritten =
		SOTMPlayerStatePrivate::SetTransformProperty(
			SaveObject,
			SOTMPlayerStatePrivate::CheckpointTransformProperty,
			CheckpointState.RespawnTransform);
	const bool bAvailableCoinsWritten =
		SOTMPlayerStatePrivate::SetIntProperty(
			SaveObject,
			SOTMPlayerStatePrivate::AvailableCoinsProperty,
			AvailableCoins);
	const bool bLifetimeCoinsWritten =
		SOTMPlayerStatePrivate::SetIntProperty(
			SaveObject,
			SOTMPlayerStatePrivate::LifetimeCoinsCollectedProperty,
			LifetimeCoinsCollected);
	TArray<FGuid> SortedCoinIds = CollectedCoinIds.Array();
	SortedCoinIds.Sort([](const FGuid& Left, const FGuid& Right)
	{
		return Left.ToString(EGuidFormats::Digits) < Right.ToString(EGuidFormats::Digits);
	});
	TArray<FString> SerializedCoinIds;
	SerializedCoinIds.Reserve(SortedCoinIds.Num());
	for (const FGuid& CoinId : SortedCoinIds)
	{
		SerializedCoinIds.Add(CoinId.ToString(EGuidFormats::DigitsWithHyphens));
	}
	// The existing SaveGame Blueprint exposes one versioned persistent string for
	// stable Coin IDs. Append a namespaced Phase 3 record rather than creating a
	// second save file or requiring a destructive SaveGame Blueprint reparent.
	SerializedCoinIds.Add(FString::Printf(TEXT("%s%d|%d"),
		*SOTMPlayerStatePrivate::SpeedBoostRecordPrefix,
		bSpeedBoostUnlocked ? 1 : 0,
		SpeedBoostLevel));
	SerializedCoinIds.Add(FString::Printf(TEXT("%s%d|%d|%d|%d"),
		*SOTMPlayerStatePrivate::Phase4RecordPrefix,
		bPhase4ChestOpened ? 1 : 0,
		bPhase4HasGateKey ? 1 : 0,
		bPhase4GateUnlocked ? 1 : 0,
		bPhase4DemoCompleted ? 1 : 0));
	SerializedCoinIds.Add(FString::Printf(TEXT("%s%d"),
		*SOTMPlayerStatePrivate::LightningThrowRecordPrefix,
		bLightningThrowUnlocked ? 1 : 0));
	TArray<FString> SerializedSkillUpgrades;
	SerializedSkillUpgrades.Reserve(SkillUpgradeLevels.Num());
	for (const TPair<FName, int32>& Upgrade : SkillUpgradeLevels)
	{
		if (Upgrade.Value > 0)
		{
			SerializedSkillUpgrades.Add(FString::Printf(TEXT("%s:%d"), *Upgrade.Key.ToString(), Upgrade.Value));
		}
	}
	SerializedCoinIds.Add(FString::Printf(TEXT("%s%d|%s"),
		*SOTMPlayerStatePrivate::SkillTreeRecordPrefix,
		AbilityPoints,
		*FString::Join(SerializedSkillUpgrades, TEXT(","))));
	SerializedCoinIds.Add(FString::Printf(TEXT("%s%d|%d|%d"),
		*SOTMPlayerStatePrivate::IsabelGateRecordPrefix,
		bIsabelGateReached ? 1 : 0,
		bHasIsabelGateKey ? 1 : 0,
		bIsabelGateUnlocked ? 1 : 0));
	const bool bCollectedCoinIdsWritten =
		SOTMPlayerStatePrivate::SetStringProperty(
			SaveObject,
			SOTMPlayerStatePrivate::CollectedCoinIdsProperty,
			FString::Join(SerializedCoinIds, TEXT("\n")));

	const bool bSchemaComplete =
		bHasStateWritten &&
		bVersionWritten &&
		bLivesWritten &&
		bMaximumLivesWritten &&
		bHealthWritten &&
		bCheckpointIdWritten &&
		bCheckpointMapWritten &&
		bCheckpointTransformWritten &&
		bAvailableCoinsWritten &&
		bLifetimeCoinsWritten &&
		bCollectedCoinIdsWritten;

	if (!bSchemaComplete)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("SOTM Player State save schema mismatch: HasState=%d Version=%d Lives=%d MaxLives=%d Health=%d CheckpointId=%d CheckpointMap=%d Transform=%d AvailableCoins=%d LifetimeCoins=%d CoinIds=%d"),
			bHasStateWritten,
			bVersionWritten,
			bLivesWritten,
			bMaximumLivesWritten,
			bHealthWritten,
			bCheckpointIdWritten,
			bCheckpointMapWritten,
			bCheckpointTransformWritten,
			bAvailableCoinsWritten,
			bLifetimeCoinsWritten,
			bCollectedCoinIdsWritten);
	}

	return bSchemaComplete;
}

bool USOTMPlayerStateSubsystem::ReadStateFromSaveObject(UObject* SaveObject)
{
	if (!SaveObject)
	{
		return false;
	}

	bool bHasState = false;
	int32 SaveVersion = 0;
	if (!SOTMPlayerStatePrivate::GetBoolProperty(
			SaveObject,
			SOTMPlayerStatePrivate::HasStateProperty,
			bHasState) ||
		!bHasState ||
		!SOTMPlayerStatePrivate::GetIntProperty(
			SaveObject,
			SOTMPlayerStatePrivate::SaveVersionProperty,
			SaveVersion) ||
		SaveVersion <= 0 ||
		SaveVersion > CurrentSaveVersion)
	{
		return false;
	}

	int32 LoadedLives = CurrentLives;
	int32 LoadedMaximumLives = MaximumLives;
	float LoadedHealth = PersistentHealth;
	FName LoadedCheckpointId = NAME_None;
	FName LoadedCheckpointMap = NAME_None;
	FTransform LoadedTransform = FTransform::Identity;
	int32 LoadedAvailableCoins = 0;
	int32 LoadedLifetimeCoins = 0;
	bool bLoadedSpeedBoostUnlocked = false;
	int32 LoadedSpeedBoostLevel = 0;
	bool bLoadedLightningThrowUnlocked = false;
	bool bLoadedPhase4ChestOpened = false;
	bool bLoadedPhase4HasGateKey = false;
	bool bLoadedPhase4GateUnlocked = false;
	bool bLoadedPhase4DemoCompleted = false;
	bool bLoadedIsabelGateReached = false;
	bool bLoadedHasIsabelGateKey = false;
	bool bLoadedIsabelGateUnlocked = false;
	int32 LoadedAbilityPoints = 0;
	TMap<FName, int32> LoadedSkillUpgradeLevels;
	FString LoadedCoinIds;

	SOTMPlayerStatePrivate::GetIntProperty(SaveObject, SOTMPlayerStatePrivate::CurrentLivesProperty, LoadedLives);
	SOTMPlayerStatePrivate::GetIntProperty(SaveObject, SOTMPlayerStatePrivate::MaximumLivesProperty, LoadedMaximumLives);
	SOTMPlayerStatePrivate::GetFloatProperty(SaveObject, SOTMPlayerStatePrivate::CurrentHealthProperty, LoadedHealth);
	SOTMPlayerStatePrivate::GetNameProperty(SaveObject, SOTMPlayerStatePrivate::CheckpointIdProperty, LoadedCheckpointId);
	SOTMPlayerStatePrivate::GetNameProperty(SaveObject, SOTMPlayerStatePrivate::CheckpointMapProperty, LoadedCheckpointMap);
	SOTMPlayerStatePrivate::GetTransformProperty(
		SaveObject,
		SOTMPlayerStatePrivate::CheckpointTransformProperty,
		LoadedTransform);

	// Schema 1 saves predate Coin persistence. Missing Phase 2 fields are an
	// accepted migration path and intentionally default to zero/empty.
	if (SaveVersion >= 2)
	{
		SOTMPlayerStatePrivate::GetIntProperty(
			SaveObject,
			SOTMPlayerStatePrivate::AvailableCoinsProperty,
			LoadedAvailableCoins);
		SOTMPlayerStatePrivate::GetIntProperty(
			SaveObject,
			SOTMPlayerStatePrivate::LifetimeCoinsCollectedProperty,
			LoadedLifetimeCoins);
		SOTMPlayerStatePrivate::GetStringProperty(
			SaveObject,
			SOTMPlayerStatePrivate::CollectedCoinIdsProperty,
			LoadedCoinIds);
	}
	// Version 2 had no spending transaction. If an older Blueprint/save omitted
	// AvailableCoins but retained lifetime progress, migrate that balance once;
	// version 3 saves never repeat this credit after a purchase.
	if (SaveVersion == 2 && LoadedAvailableCoins <= 0 && LoadedLifetimeCoins > 0)
	{
		LoadedAvailableCoins = LoadedLifetimeCoins;
	}

	const USOTMPlayerSystemSettings* Settings = GetDefault<USOTMPlayerSystemSettings>();
	MaximumLives = FMath::Max(1, LoadedMaximumLives);
	CurrentLives = FMath::Clamp(LoadedLives, 0, MaximumLives);
	PersistentHealth = FMath::Clamp(LoadedHealth, 0.0f, FMath::Max(1.0f, Settings->MaximumHealth));
	AvailableCoins = FMath::Max(0, LoadedAvailableCoins);
	LifetimeCoinsCollected = FMath::Max(AvailableCoins, LoadedLifetimeCoins);
	CollectedCoinIds.Reset();
	TArray<FString> LoadedCoinIdStrings;
	LoadedCoinIds.ParseIntoArrayLines(LoadedCoinIdStrings, true);
	for (const FString& SerializedId : LoadedCoinIdStrings)
	{
		if (SaveVersion >= 3 && SerializedId.StartsWith(SOTMPlayerStatePrivate::SpeedBoostRecordPrefix))
		{
			TArray<FString> Fields;
			SerializedId.ParseIntoArray(Fields, TEXT("|"), false);
			if (Fields.Num() == 3)
			{
				bLoadedSpeedBoostUnlocked = FCString::Atoi(*Fields[1]) != 0;
				LoadedSpeedBoostLevel = FMath::Max(0, FCString::Atoi(*Fields[2]));
			}
			continue;
		}
		if (SaveVersion >= 5 && SerializedId.StartsWith(SOTMPlayerStatePrivate::LightningThrowRecordPrefix))
		{
			TArray<FString> Fields;
			SerializedId.ParseIntoArray(Fields, TEXT("|"), false);
			if (Fields.Num() == 2)
			{
				bLoadedLightningThrowUnlocked = FCString::Atoi(*Fields[1]) != 0;
			}
			continue;
		}
		if (SaveVersion >= 6 && SerializedId.StartsWith(SOTMPlayerStatePrivate::SkillTreeRecordPrefix))
		{
			const FString Remainder = SerializedId.RightChop(SOTMPlayerStatePrivate::SkillTreeRecordPrefix.Len());
			FString PointsPart;
			FString UpgradesPart;
			if (Remainder.Split(TEXT("|"), &PointsPart, &UpgradesPart))
			{
				LoadedAbilityPoints = FMath::Max(0, FCString::Atoi(*PointsPart));
				TArray<FString> UpgradeEntries;
				UpgradesPart.ParseIntoArray(UpgradeEntries, TEXT(","), true);
				for (const FString& Entry : UpgradeEntries)
				{
					FString AbilityIdPart;
					FString LevelPart;
					if (Entry.Split(TEXT(":"), &AbilityIdPart, &LevelPart))
					{
						const int32 UpgradeLevel = FMath::Max(0, FCString::Atoi(*LevelPart));
						if (UpgradeLevel > 0)
						{
							LoadedSkillUpgradeLevels.Add(FName(*AbilityIdPart), UpgradeLevel);
						}
					}
				}
			}
			continue;
		}
		if (SaveVersion >= 4 && SerializedId.StartsWith(SOTMPlayerStatePrivate::Phase4RecordPrefix))
		{
			TArray<FString> Fields;
			SerializedId.ParseIntoArray(Fields, TEXT("|"), false);
			if (Fields.Num() == 5)
			{
				bLoadedPhase4ChestOpened = FCString::Atoi(*Fields[1]) != 0;
				bLoadedPhase4HasGateKey = FCString::Atoi(*Fields[2]) != 0;
				bLoadedPhase4GateUnlocked = FCString::Atoi(*Fields[3]) != 0;
				bLoadedPhase4DemoCompleted = FCString::Atoi(*Fields[4]) != 0;
			}
			continue;
		}
		if (SaveVersion >= 7 && SerializedId.StartsWith(SOTMPlayerStatePrivate::IsabelGateRecordPrefix))
		{
			TArray<FString> Fields;
			SerializedId.ParseIntoArray(Fields, TEXT("|"), false);
			if (Fields.Num() == 4)
			{
				bLoadedIsabelGateReached = FCString::Atoi(*Fields[1]) != 0;
				bLoadedHasIsabelGateKey = FCString::Atoi(*Fields[2]) != 0;
				bLoadedIsabelGateUnlocked = FCString::Atoi(*Fields[3]) != 0;
			}
			continue;
		}
		FGuid CoinId;
		if (FGuid::Parse(SerializedId, CoinId) && CoinId.IsValid())
		{
			CollectedCoinIds.Add(CoinId);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("SOTM Coin: ignored invalid collected ID in save: %s"), *SerializedId);
		}
	}
	// Apply ownership only after the namespaced Phase 3 record has been parsed.
	// Assigning these fields before the loop would always restore the defaults
	// (locked/level 0) even when the save contained a valid unlock record.
	bSpeedBoostUnlocked = bLoadedSpeedBoostUnlocked && LoadedSpeedBoostLevel > 0;
	SpeedBoostLevel = bSpeedBoostUnlocked ? FMath::Max(1, LoadedSpeedBoostLevel) : 0;
	bLightningThrowUnlocked = bLoadedLightningThrowUnlocked && bSpeedBoostUnlocked;
	bPhase4ChestOpened = bLoadedPhase4ChestOpened;
	bPhase4HasGateKey = bLoadedPhase4HasGateKey || bPhase4ChestOpened;
	bPhase4GateUnlocked = bLoadedPhase4GateUnlocked;
	bPhase4DemoCompleted = bLoadedPhase4DemoCompleted;
	bIsabelGateReached = bLoadedIsabelGateReached;
	bHasIsabelGateKey = bLoadedHasIsabelGateKey;
	bIsabelGateUnlocked = bLoadedIsabelGateUnlocked;
	AbilityPoints = LoadedAbilityPoints;
	SkillUpgradeLevels = LoadedSkillUpgradeLevels;
	bCoinStateDirty = false;

	CheckpointState.bIsValid = !LoadedCheckpointId.IsNone() && !LoadedCheckpointMap.IsNone();
	CheckpointState.CheckpointId = LoadedCheckpointId;
	CheckpointState.MapPackageName = LoadedCheckpointMap;
	CheckpointState.RespawnTransform = LoadedTransform;
	bGameOver = CurrentLives <= 0;
	OnCoinsChanged.Broadcast(AvailableCoins, LifetimeCoinsCollected);
	OnSpeedBoostOwnershipChanged.Broadcast(bSpeedBoostUnlocked, SpeedBoostLevel);
	OnLightningThrowOwnershipChanged.Broadcast(bLightningThrowUnlocked);
	OnPhase4ProgressChanged.Broadcast(
		bPhase4ChestOpened, bPhase4HasGateKey, bPhase4GateUnlocked, bPhase4DemoCompleted);
	return true;
}

FName USOTMPlayerStateSubsystem::NormalizeMapPackageName(const UWorld* World)
{
	if (!World)
	{
		return NAME_None;
	}

	const FString PackageName = World->GetOutermost()->GetName();
	const FString LongPath = FPackageName::GetLongPackagePath(PackageName);
	FString ShortName = FPackageName::GetShortName(PackageName);

	if (ShortName.StartsWith(TEXT("UEDPIE_")))
	{
		const int32 PrefixEnd = ShortName.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 7);
		if (PrefixEnd != INDEX_NONE)
		{
			ShortName = ShortName.Mid(PrefixEnd + 1);
		}
	}

	return FName(*(LongPath / ShortName));
}
