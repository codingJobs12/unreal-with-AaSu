#include "Objective/SOTMObjectiveSubsystem.h"

#include "Engine/GameInstance.h"
#include "SOTMPlayerStateSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMObjective, Log, All);

const FName USOTMObjectiveSubsystem::CollectAllForestCoinsId(TEXT("CollectAllForestCoins"));

void USOTMObjectiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<USOTMPlayerStateSubsystem>();

	CollectAllForestCoins.ObjectiveId = CollectAllForestCoinsId;
	CollectAllForestCoins.DisplayName = NSLOCTEXT("SOTM", "CollectAllForestCoins", "Collect All Coins");
	CollectAllForestCoins.RequiredProgress = TotalForestCoins;
	CollectAllForestCoins.State = ESOTMObjectiveState::Locked;

	PlayerState = GetGameInstance() ? GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	if (PlayerState)
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		PlayerState->OnCoinsChanged.AddDynamic(this, &ThisClass::HandleCoinsChanged);
	}
	RefreshFromPersistentCoinState(false);
}

void USOTMObjectiveSubsystem::Deinitialize()
{
	if (PlayerState)
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
	}
	PlayerState = nullptr;
	Super::Deinitialize();
}

void USOTMObjectiveSubsystem::SetForestObjectiveActive(const bool bActive)
{
	if (bForestObjectiveActive == bActive)
	{
		RefreshFromPersistentCoinState(true);
		return;
	}

	bForestObjectiveActive = bActive;
	RefreshFromPersistentCoinState(true);
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

