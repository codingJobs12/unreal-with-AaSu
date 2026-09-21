#pragma once

#include "CoreMinimal.h"
#include "SOTMLightningThrowTypes.generated.h"

UENUM(BlueprintType)
enum class ESOTMLightningThrowRuntimeState : uint8
{
	Locked,
	Ready,
	Cooldown
};

UENUM(BlueprintType)
enum class ESOTMLightningThrowPurchaseResult : uint8
{
	Success,
	AlreadyOwned,
	ObjectiveIncomplete,
	SpeedBoostMissing,
	NotEnoughCoins,
	InvalidCost,
	NoActiveSave,
	SaveFailed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FSOTMLightningThrowOwnershipChangedSignature,
	bool, bUnlocked);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSOTMLightningThrowStateChangedSignature,
	ESOTMLightningThrowRuntimeState, State,
	float, RemainingSeconds,
	float, NormalizedRemaining);
