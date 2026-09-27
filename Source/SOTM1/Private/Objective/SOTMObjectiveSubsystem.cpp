#include "Objective/SOTMObjectiveSubsystem.h"

#include "Engine/GameInstance.h"
#include "SOTMPlayerStateSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMObjective, Log, All);

const FName USOTMObjectiveSubsystem::CollectAllForestCoinsId(TEXT("CollectAllForestCoins"));
const FName USOTMObjectiveSubsystem::UnlockSpeedBoostId(TEXT("UnlockSpeedBoost"));
const FName USOTMObjectiveSubsystem::UnlockLightningThrowId(TEXT("UnlockLightningThrow"));
const FName USOTMObjectiveSubsystem::FindChestId(TEXT("FindChest"));
const FName USOTMObjectiveSubsystem::ObtainGateKeyId(TEXT("ObtainGateKey"));
const FName USOTMObjectiveSubsystem::ReachGateId(TEXT("ReachGate"));
const FName USOTMObjectiveSubsystem::DemoCompleteId(TEXT("DemoComplete"));
const FName USOTMObjectiveSubsystem::ReachIsabelGateId(TEXT("ReachIsabelGate"));
const FName USOTMObjectiveSubsystem::UnlockIsabelGateId(TEXT("UnlockIsabelGate"));
const FName USOTMObjectiveSubsystem::DefeatIsabelId(TEXT("DefeatIsabel"));

void USOTMObjectiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USOTMPlayerStateSubsystem>();

	CollectAllForestCoins.ObjectiveId = CollectAllForestCoinsId;
	CollectAllForestCoins.DisplayName = NSLOCTEXT("SOTM", "CollectAllForestCoins", "Collect All Coins");
	CollectAllForestCoins.RequiredProgress = TotalForestCoins;
	CollectAllForestCoins.State = ESOTMObjectiveState::Locked;
	auto InitializeBinaryObjective = [](FSOTMObjectiveData& Objective, const FName Id, const FText& Name)
	{
		Objective.ObjectiveId = Id;
		Objective.DisplayName = Name;
		Objective.CurrentProgress = 0;
		Objective.RequiredProgress = 1;
		Objective.State = ESOTMObjectiveState::Locked;
	};
	InitializeBinaryObjective(UnlockSpeedBoost, UnlockSpeedBoostId,
		NSLOCTEXT("SOTM", "UnlockSpeedBoost", "Unlock Speed Boost"));
	InitializeBinaryObjective(UnlockLightningThrow, UnlockLightningThrowId,
		NSLOCTEXT("SOTM", "UnlockLightningThrow", "Unlock Lightning Throw"));
	InitializeBinaryObjective(FindChest, FindChestId,
		NSLOCTEXT("SOTM", "FindChest", "Find the Chest"));
	InitializeBinaryObjective(ObtainGateKey, ObtainGateKeyId,
		NSLOCTEXT("SOTM", "ObtainGateKey", "Obtain Gate Key"));
	InitializeBinaryObjective(ReachGate, ReachGateId,
		NSLOCTEXT("SOTM", "ReachGate", "Reach the Gate"));
	InitializeBinaryObjective(DemoComplete, DemoCompleteId,
		NSLOCTEXT("SOTM", "DemoCompleteObjective", "Demo Complete"));
	InitializeBinaryObjective(ReachIsabelGate, ReachIsabelGateId,
		NSLOCTEXT("SOTM", "ReachIsabelGate", "Reach the Gate"));
	InitializeBinaryObjective(UnlockIsabelGate, UnlockIsabelGateId,
		NSLOCTEXT("SOTM", "UnlockIsabelGate", "Unlock the Gate with Key"));
	InitializeBinaryObjective(DefeatIsabel, DefeatIsabelId,
		NSLOCTEXT("SOTM", "DefeatIsabel", "Defeat Isabel"));

	PlayerState = GetGameInstance() ? GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	if (PlayerState)
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		PlayerState->OnCoinsChanged.AddDynamic(this, &ThisClass::HandleCoinsChanged);
		PlayerState->OnSpeedBoostOwnershipChanged.AddUniqueDynamic(this, &ThisClass::HandleSpeedBoostChanged);
		PlayerState->OnPhase4ProgressChanged.AddUniqueDynamic(this, &ThisClass::HandlePhase4ProgressChanged);
		PlayerState->OnIsabelGateProgressChanged.AddUniqueDynamic(this, &ThisClass::HandleIsabelGateProgressChanged);
	}
	RefreshFromPersistentCoinState(false);
	RefreshPhase4Objectives(false);
	RefreshIsabelGateObjectives(false);
}

void USOTMObjectiveSubsystem::Deinitialize()
{
	if (PlayerState)
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		PlayerState->OnSpeedBoostOwnershipChanged.RemoveDynamic(this, &ThisClass::HandleSpeedBoostChanged);
		PlayerState->OnPhase4ProgressChanged.RemoveDynamic(this, &ThisClass::HandlePhase4ProgressChanged);
		PlayerState->OnIsabelGateProgressChanged.RemoveDynamic(this, &ThisClass::HandleIsabelGateProgressChanged);
	}
	PlayerState = nullptr;
	Super::Deinitialize();
}

void USOTMObjectiveSubsystem::SetForestObjectiveActive(const bool bActive)
{
	if (bForestObjectiveActive == bActive)
	{
		RefreshFromPersistentCoinState(true);
		RefreshPhase4Objectives(true);
		RefreshIsabelGateObjectives(true);
		return;
	}

	bForestObjectiveActive = bActive;
	RefreshFromPersistentCoinState(true);
	RefreshPhase4Objectives(true);
	RefreshIsabelGateObjectives(true);
	UE_LOG(LogSOTMObjective, Display, TEXT("Forest objective presentation %s."),
		bForestObjectiveActive ? TEXT("activated") : TEXT("deactivated"));
}

void USOTMObjectiveSubsystem::HandleCoinsChanged(
	const int32 AvailableCoins,
	const int32 LifetimeCoinsCollected)
{
	(void)AvailableCoins;
	(void)LifetimeCoinsCollected;
	RefreshFromPersistentCoinState(false);
	RefreshPhase4Objectives(false);
	RefreshIsabelGateObjectives(false);
}

void USOTMObjectiveSubsystem::HandleSpeedBoostChanged(const bool bUnlocked, const int32 Level)
{
	(void)bUnlocked;
	(void)Level;
	RefreshPhase4Objectives(false);
	RefreshIsabelGateObjectives(false);
}

void USOTMObjectiveSubsystem::HandlePhase4ProgressChanged(
	const bool bChestOpened,
	const bool bHasGateKey,
	const bool bGateUnlocked,
	const bool bDemoCompleted)
{
	(void)bChestOpened;
	(void)bHasGateKey;
	(void)bGateUnlocked;
	(void)bDemoCompleted;
	RefreshPhase4Objectives(false);
	RefreshIsabelGateObjectives(false);
}

void USOTMObjectiveSubsystem::HandleIsabelGateProgressChanged(
	const bool bReached,
	const bool bHasKey,
	const bool bUnlocked)
{
	(void)bReached;
	(void)bHasKey;
	(void)bUnlocked;
	RefreshIsabelGateObjectives(false);
}

TArray<FSOTMObjectiveData> USOTMObjectiveSubsystem::GetChapterOneObjectives() const
{
	// FindChest/ObtainGateKey stay - collecting the key from the chest is still
	// part of the flow, unchanged from before. Only ReachGate/DemoComplete are
	// left out: those belonged to a second, separate gate object that no
	// longer exists in the level, and duplicated ReachIsabelGate/UnlockIsabelGate
	// below, which now represent the one real gate.
	return { CollectAllForestCoins, UnlockSpeedBoost, UnlockLightningThrow,
		FindChest, ObtainGateKey,
		ReachIsabelGate, UnlockIsabelGate, DefeatIsabel };
}

FSOTMObjectiveData USOTMObjectiveSubsystem::GetActiveChapterOneObjective() const
{
	for (const FSOTMObjectiveData& Objective : GetChapterOneObjectives())
	{
		if (Objective.State == ESOTMObjectiveState::Active)
		{
			return Objective;
		}
	}
	return DemoComplete.State == ESOTMObjectiveState::Completed ? DemoComplete : FSOTMObjectiveData();
}

ESOTMPhase4ActionResult USOTMObjectiveSubsystem::TryOpenPhase4Chest()
{
	RefreshFromPersistentCoinState(false);
	RefreshPhase4Objectives(false);
	if (!PlayerState)
	{
		return ESOTMPhase4ActionResult::InvalidState;
	}
	if (PlayerState->IsPhase4ChestOpened())
	{
		return ESOTMPhase4ActionResult::AlreadyCompleted;
	}
	if (CollectAllForestCoins.State != ESOTMObjectiveState::Completed)
	{
		return ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;
	}
	if (!PlayerState->IsSpeedBoostUnlocked())
	{
		return ESOTMPhase4ActionResult::MissingSpeedBoost;
	}
	// Design doc: "That key leads to her true form. You'll need both abilities to survive."
	if (!PlayerState->IsLightningThrowUnlocked())
	{
		return ESOTMPhase4ActionResult::MissingLightningThrow;
	}
	// Opening the chest only reveals the key now - the player still has to walk up to it and
	// press E to actually collect it (see TryCollectPhase4GateKey).
	return PlayerState->CommitPhase4ChestOpened()
		? ESOTMPhase4ActionResult::Success
		: ESOTMPhase4ActionResult::SaveFailed;
}

ESOTMPhase4ActionResult USOTMObjectiveSubsystem::TryCollectPhase4GateKey()
{
	if (!PlayerState)
	{
		return ESOTMPhase4ActionResult::InvalidState;
	}
	if (PlayerState->HasPhase4GateKey())
	{
		return ESOTMPhase4ActionResult::AlreadyCompleted;
	}
	// The key can't be collected before the chest that reveals it has been opened.
	if (!PlayerState->IsPhase4ChestOpened())
	{
		return ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;
	}
	return PlayerState->CommitPhase4GateKey()
		? ESOTMPhase4ActionResult::Success
		: ESOTMPhase4ActionResult::SaveFailed;
}

ESOTMPhase4ActionResult USOTMObjectiveSubsystem::TryUnlockPhase4Gate()
{
	RefreshFromPersistentCoinState(false);
	RefreshPhase4Objectives(false);
	if (!PlayerState)
	{
		return ESOTMPhase4ActionResult::InvalidState;
	}
	if (PlayerState->IsPhase4GateUnlocked())
	{
		return ESOTMPhase4ActionResult::AlreadyCompleted;
	}
	if (CollectAllForestCoins.State != ESOTMObjectiveState::Completed)
	{
		return ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;
	}
	if (!PlayerState->IsSpeedBoostUnlocked())
	{
		return ESOTMPhase4ActionResult::MissingSpeedBoost;
	}
	if (!PlayerState->IsLightningThrowUnlocked())
	{
		return ESOTMPhase4ActionResult::MissingLightningThrow;
	}
	if (!PlayerState->HasPhase4GateKey())
	{
		return ESOTMPhase4ActionResult::MissingGateKey;
	}
	return PlayerState->CommitPhase4GateUnlocked()
		? ESOTMPhase4ActionResult::Success
		: ESOTMPhase4ActionResult::SaveFailed;
}

ESOTMPhase4ActionResult USOTMObjectiveSubsystem::TryCompletePhase4Demo()
{
	if (!PlayerState)
	{
		return ESOTMPhase4ActionResult::InvalidState;
	}
	if (PlayerState->IsPhase4DemoCompleted())
	{
		return ESOTMPhase4ActionResult::AlreadyCompleted;
	}
	if (!PlayerState->IsPhase4GateUnlocked())
	{
		return ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;
	}
	return PlayerState->CommitPhase4DemoCompleted()
		? ESOTMPhase4ActionResult::Success
		: ESOTMPhase4ActionResult::SaveFailed;
}

ESOTMPhase4ActionResult USOTMObjectiveSubsystem::TryReachIsabelGate()
{
	RefreshPhase4Objectives(false);
	RefreshIsabelGateObjectives(false);
	if (!PlayerState)
	{
		return ESOTMPhase4ActionResult::InvalidState;
	}
	if (PlayerState->IsIsabelGateReached())
	{
		return ESOTMPhase4ActionResult::AlreadyCompleted;
	}
	// Only counts once the key is already held - just walking up to the gate
	// before collecting the chest key must NOT complete this objective. The
	// gate actor also retries this the moment the key is collected while the
	// player is still standing in range, so the player never has to leave and
	// re-enter the overlap box to pick it up.
	if (!PlayerState->HasPhase4GateKey())
	{
		return ESOTMPhase4ActionResult::MissingGateKey;
	}
	return PlayerState->CommitIsabelGateReached()
		? ESOTMPhase4ActionResult::Success
		: ESOTMPhase4ActionResult::SaveFailed;
}

ESOTMPhase4ActionResult USOTMObjectiveSubsystem::TryUnlockIsabelGate()
{
	RefreshIsabelGateObjectives(false);
	if (!PlayerState)
	{
		return ESOTMPhase4ActionResult::InvalidState;
	}
	if (PlayerState->IsIsabelGateUnlocked())
	{
		return ESOTMPhase4ActionResult::AlreadyCompleted;
	}
	if (!PlayerState->IsIsabelGateReached())
	{
		return ESOTMPhase4ActionResult::PreviousObjectivesIncomplete;
	}
	if (!PlayerState->HasPhase4GateKey())
	{
		return ESOTMPhase4ActionResult::MissingGateKey;
	}
	return PlayerState->CommitIsabelGateUnlocked()
		? ESOTMPhase4ActionResult::Success
		: ESOTMPhase4ActionResult::SaveFailed;
}

FText USOTMObjectiveSubsystem::GetGateRequirementFeedback() const
{
	const bool bCoins = CollectAllForestCoins.State == ESOTMObjectiveState::Completed;
	const bool bBoost = PlayerState && PlayerState->IsSpeedBoostUnlocked();
	const bool bKey = PlayerState && PlayerState->HasPhase4GateKey();
	return FText::Format(
		NSLOCTEXT("SOTM", "GateRequirements", "GATE LOCKED\n{0} Collect All Coins\n{1} Unlock Speed Boost\n{2} Gate Key"),
		bCoins ? FText::FromString(TEXT("[DONE]")) : FText::FromString(TEXT("[ ]")),
		bBoost ? FText::FromString(TEXT("[DONE]")) : FText::FromString(TEXT("[ ]")),
		bKey ? FText::FromString(TEXT("[DONE]")) : FText::FromString(TEXT("[ ]")));
}

void USOTMObjectiveSubsystem::RefreshFromPersistentCoinState(const bool bForceBroadcast)
{
	const FSOTMObjectiveData Previous = CollectAllForestCoins;
	const int32 Lifetime = PlayerState ? PlayerState->GetLifetimeCoinsCollected() : 0;
	const int32 UniqueCount = PlayerState ? PlayerState->GetCollectedCoinCount() : 0;

	// Lifetime is the currency-independent completion measure. The unique set is
	// also persistent and guards the normal one-Coin-per-GUID production route.
	const int32 AuthoritativeProgress = FMath::Max(Lifetime, UniqueCount);
	CollectAllForestCoins.CurrentProgress = FMath::Clamp(AuthoritativeProgress, 0, TotalForestCoins);
	CollectAllForestCoins.State = CollectAllForestCoins.CurrentProgress >= TotalForestCoins
		? ESOTMObjectiveState::Completed
		: (bForestObjectiveActive ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);

	const bool bChanged = Previous.CurrentProgress != CollectAllForestCoins.CurrentProgress ||
		Previous.State != CollectAllForestCoins.State;
	if (bChanged || bForceBroadcast)
	{
		OnObjectiveChanged.Broadcast(CollectAllForestCoins);
		UE_LOG(LogSOTMObjective, Display, TEXT("Objective %s: %d/%d state=%d lifetime=%d unique=%d"),
			*CollectAllForestCoins.ObjectiveId.ToString(),
			CollectAllForestCoins.CurrentProgress,
			CollectAllForestCoins.RequiredProgress,
			static_cast<int32>(CollectAllForestCoins.State),
			Lifetime,
			UniqueCount);
	}
}

void USOTMObjectiveSubsystem::RefreshPhase4Objectives(const bool bForceBroadcast)
{
	const FSOTMObjectiveData PreviousUnlock = UnlockSpeedBoost;
	const FSOTMObjectiveData PreviousLightning = UnlockLightningThrow;
	const FSOTMObjectiveData PreviousChest = FindChest;
	const FSOTMObjectiveData PreviousKey = ObtainGateKey;
	const FSOTMObjectiveData PreviousGate = ReachGate;
	const FSOTMObjectiveData PreviousDemo = DemoComplete;

	const bool bCoinsComplete = CollectAllForestCoins.State == ESOTMObjectiveState::Completed;
	const bool bBoost = PlayerState && PlayerState->IsSpeedBoostUnlocked();
	const bool bLightning = PlayerState && PlayerState->IsLightningThrowUnlocked();
	const bool bChest = PlayerState && PlayerState->IsPhase4ChestOpened();
	const bool bKey = PlayerState && PlayerState->HasPhase4GateKey();
	const bool bGate = PlayerState && PlayerState->IsPhase4GateUnlocked();
	const bool bDemo = PlayerState && PlayerState->IsPhase4DemoCompleted();

	UnlockSpeedBoost.CurrentProgress = bBoost ? 1 : 0;
	UnlockSpeedBoost.State = bBoost ? ESOTMObjectiveState::Completed
		: (bForestObjectiveActive && bCoinsComplete ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);
	UnlockLightningThrow.CurrentProgress = bLightning ? 1 : 0;
	UnlockLightningThrow.State = bLightning ? ESOTMObjectiveState::Completed
		: (bForestObjectiveActive && bCoinsComplete && bBoost ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);
	FindChest.CurrentProgress = bChest ? 1 : 0;
	FindChest.State = bChest ? ESOTMObjectiveState::Completed
		: (bForestObjectiveActive && bCoinsComplete && bBoost && bLightning
			? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);
	ObtainGateKey.CurrentProgress = bKey ? 1 : 0;
	ObtainGateKey.State = bKey ? ESOTMObjectiveState::Completed
		: (bChest ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);
	ReachGate.CurrentProgress = bGate ? 1 : 0;
	ReachGate.State = bGate ? ESOTMObjectiveState::Completed
		: (bForestObjectiveActive && bCoinsComplete && bBoost && bLightning && bKey
			? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);
	DemoComplete.CurrentProgress = bDemo ? 1 : 0;
	DemoComplete.State = bDemo ? ESOTMObjectiveState::Completed
		: (bGate ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);

	BroadcastIfChanged(PreviousUnlock, UnlockSpeedBoost, bForceBroadcast);
	BroadcastIfChanged(PreviousLightning, UnlockLightningThrow, bForceBroadcast);
	BroadcastIfChanged(PreviousChest, FindChest, bForceBroadcast);
	BroadcastIfChanged(PreviousKey, ObtainGateKey, bForceBroadcast);
	BroadcastIfChanged(PreviousGate, ReachGate, bForceBroadcast);
	BroadcastIfChanged(PreviousDemo, DemoComplete, bForceBroadcast);
}

void USOTMObjectiveSubsystem::RefreshIsabelGateObjectives(const bool bForceBroadcast)
{
	// Standalone chain, independent of the Phase 4 demo sequence - "Reach the Gate" is
	// always available so overlapping the Mansion Gate completes it right away, and it
	// shares the same gate key the Phase 4 chest grants (HasPhase4GateKey).
	const FSOTMObjectiveData PreviousReach = ReachIsabelGate;
	const FSOTMObjectiveData PreviousUnlockGate = UnlockIsabelGate;
	const FSOTMObjectiveData PreviousDefeat = DefeatIsabel;

	const bool bReached = PlayerState && PlayerState->IsIsabelGateReached();
	const bool bUnlocked = PlayerState && PlayerState->IsIsabelGateUnlocked();

	ReachIsabelGate.CurrentProgress = bReached ? 1 : 0;
	ReachIsabelGate.State = bReached ? ESOTMObjectiveState::Completed : ESOTMObjectiveState::Active;
	UnlockIsabelGate.CurrentProgress = bUnlocked ? 1 : 0;
	UnlockIsabelGate.State = bUnlocked ? ESOTMObjectiveState::Completed
		: (bReached ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked);
	DefeatIsabel.CurrentProgress = 0;
	DefeatIsabel.State = bUnlocked ? ESOTMObjectiveState::Active : ESOTMObjectiveState::Locked;

	BroadcastIfChanged(PreviousReach, ReachIsabelGate, bForceBroadcast);
	BroadcastIfChanged(PreviousUnlockGate, UnlockIsabelGate, bForceBroadcast);
	BroadcastIfChanged(PreviousDefeat, DefeatIsabel, bForceBroadcast);
}

void USOTMObjectiveSubsystem::BroadcastIfChanged(
	const FSOTMObjectiveData& Previous,
	const FSOTMObjectiveData& Current,
	const bool bForceBroadcast)
{
	if (bForceBroadcast || Previous.CurrentProgress != Current.CurrentProgress || Previous.State != Current.State)
	{
		OnObjectiveChanged.Broadcast(Current);
		UE_LOG(LogSOTMObjective, Display, TEXT("Objective %s state=%d progress=%d/%d"),
			*Current.ObjectiveId.ToString(), static_cast<int32>(Current.State),
			Current.CurrentProgress, Current.RequiredProgress);
	}
}
