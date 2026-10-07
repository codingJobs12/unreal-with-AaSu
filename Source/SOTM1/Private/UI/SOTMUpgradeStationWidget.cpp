#include "UI/SOTMUpgradeStationWidget.h"

#include "Ability/SOTMPhase3Settings.h"
#include "Ability/SOTMLightningThrowSettings.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Demo/SOTMDemoPhase3WorldSubsystem.h"
#include "Engine/GameInstance.h"
#include "Animation/WidgetAnimation.h"
#include "InputCoreTypes.h"
#include "SOTMPlayerStateSubsystem.h"

void USOTMUpgradeStationWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	bClosing = false;
	PlayerState = GetGameInstance() ? GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	ObjectiveState = GetGameInstance() ? GetGameInstance()->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;
	if (PlayerState)
	{
		PlayerState->OnCoinsChanged.AddUniqueDynamic(this, &ThisClass::HandleCoinsChanged);
		PlayerState->OnSpeedBoostOwnershipChanged.AddUniqueDynamic(this, &ThisClass::HandleOwnershipChanged);
		PlayerState->OnLightningThrowOwnershipChanged.AddUniqueDynamic(this, &ThisClass::HandleLightningOwnershipChanged);
	}
	if (ObjectiveState)
	{
		ObjectiveState->OnObjectiveChanged.AddUniqueDynamic(this, &ThisClass::HandleObjectiveChanged);
	}
	// Buttons are built in the WBP_UpgradeStation Designer canvas (BindWidgetOptional
	// above), so their click handling is wired here instead of via SAssignNew/OnClicked_UObject.
	if (UnlockButton)
	{
		UnlockButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleUnlockClicked);
	}
	if (LightningUnlockButton)
	{
		LightningUnlockButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightningUnlockClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	RefreshPresentation();
	SetKeyboardFocus();
}

void USOTMUpgradeStationWidget::NativeDestruct()
{
	if (PlayerState)
	{
		PlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		PlayerState->OnSpeedBoostOwnershipChanged.RemoveDynamic(this, &ThisClass::HandleOwnershipChanged);
		PlayerState->OnLightningThrowOwnershipChanged.RemoveDynamic(this, &ThisClass::HandleLightningOwnershipChanged);
	}
	if (ObjectiveState)
	{
		ObjectiveState->OnObjectiveChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveChanged);
	}
	if (UnlockButton)
	{
		UnlockButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnlockClicked);
	}
	if (LightningUnlockButton)
	{
		LightningUnlockButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLightningUnlockClicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleCloseClicked);
	}
	PlayerState = nullptr;
	ObjectiveState = nullptr;
	Super::NativeDestruct();
}

FReply USOTMUpgradeStationWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == EKeys::E)
	{
		HandleCloseClicked();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USOTMUpgradeStationWidget::HandleCoinsChanged(int32 AvailableCoins, int32 LifetimeCoinsCollected)
{
	(void)AvailableCoins;
	(void)LifetimeCoinsCollected;
	RefreshPresentation();
}

void USOTMUpgradeStationWidget::HandleOwnershipChanged(bool bUnlocked, int32 Level)
{
	(void)bUnlocked;
	(void)Level;
	RefreshPresentation();
}

void USOTMUpgradeStationWidget::HandleLightningOwnershipChanged(bool bUnlocked)
{
	(void)bUnlocked;
	RefreshPresentation();
}

void USOTMUpgradeStationWidget::HandleObjectiveChanged(FSOTMObjectiveData Objective)
{
	(void)Objective;
	RefreshPresentation();
}

// UnlockButton is the PRIORITY slot's button. It purchases whichever ability is
// currently the priority (Speed Boost until owned, then Lightning Throw) - see the
// class comment and RefreshPresentation() for how priority is decided.
void USOTMUpgradeStationWidget::HandleUnlockClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (USOTMDemoPhase3WorldSubsystem* Phase3 = World->GetSubsystem<USOTMDemoPhase3WorldSubsystem>())
		{
			const bool bSpeedOwned = PlayerState && PlayerState->IsSpeedBoostUnlocked();
			if (bSpeedOwned)
			{
				// Priority has moved on to Lightning Throw.
				LastLightningResult = Phase3->TryPurchaseLightningThrow();
				bHasAttemptedLightningPurchase = true;
			}
			else
			{
				LastPurchaseResult = Phase3->TryPurchaseSpeedBoost();
				bHasAttemptedPurchase = true;
			}
		}
	}
	RefreshPresentation();
}

// LightningUnlockButton is the MINIMIZED slot's button - it mirrors whichever ability
// is NOT the priority right now. Before Speed Boost is owned that's genuinely Lightning
// (button stays disabled until Speed Boost is bought - see CanPurchaseLightning); after
// Speed Boost is owned the minimized slot is Speed Boost itself, so this becomes a
// harmless no-op (already owned, always disabled) kept for symmetry.
void USOTMUpgradeStationWidget::HandleLightningUnlockClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (USOTMDemoPhase3WorldSubsystem* Phase3 = World->GetSubsystem<USOTMDemoPhase3WorldSubsystem>())
		{
			const bool bSpeedOwned = PlayerState && PlayerState->IsSpeedBoostUnlocked();
			if (bSpeedOwned)
			{
				LastPurchaseResult = Phase3->TryPurchaseSpeedBoost();
				bHasAttemptedPurchase = true;
			}
			else
			{
				LastLightningResult = Phase3->TryPurchaseLightningThrow();
				bHasAttemptedLightningPurchase = true;
			}
		}
	}
	RefreshPresentation();
}

bool USOTMUpgradeStationWidget::CanPurchaseLightning() const
{
	if (!PlayerState || PlayerState->IsLightningThrowUnlocked() || !PlayerState->IsSpeedBoostUnlocked())
	{
		return false;
	}
	const FSOTMObjectiveData Objective = ObjectiveState
		? ObjectiveState->GetCollectAllForestCoinsObjective() : FSOTMObjectiveData();
	return Objective.State == ESOTMObjectiveState::Completed &&
		PlayerState->GetAvailableCoins() >= GetDefault<USOTMLightningThrowSettings>()->LightningThrowUnlockCost;
}

FText USOTMUpgradeStationWidget::GetLightningRequirementText() const
{
	if (!PlayerState)
	{
		return FText::FromString(TEXT("PLAYER STATE UNAVAILABLE"));
	}
	if (PlayerState->IsLightningThrowUnlocked())
	{
		return FText::FromString(TEXT("OWNED - PRESS F TO STUN"));
	}
	if (!PlayerState->IsSpeedBoostUnlocked())
	{
		return FText::FromString(TEXT("UNLOCK SPEED BOOST FIRST"));
	}
	const FSOTMObjectiveData Objective = ObjectiveState
		? ObjectiveState->GetCollectAllForestCoinsObjective() : FSOTMObjectiveData();
	if (Objective.State != ESOTMObjectiveState::Completed)
	{
		return FText::FromString(TEXT("COLLECT ALL COINS FIRST"));
	}
	if (PlayerState->GetAvailableCoins() < GetDefault<USOTMLightningThrowSettings>()->LightningThrowUnlockCost)
	{
		return FText::FromString(TEXT("NOT ENOUGH COINS"));
	}
	return FText::FromString(TEXT("READY TO UNLOCK"));
}

void USOTMUpgradeStationWidget::HandleCloseClicked()
{
	if (bClosing)
	{
		return;
	}
	if (NewAnimation)
	{
		bClosing = true;
		FWidgetAnimationDynamicEvent Finished;
		Finished.BindDynamic(this, &ThisClass::HandleCloseAnimationFinished);
		BindToAnimationFinished(NewAnimation, Finished);
		PlayAnimationReverse(NewAnimation, 1.0f);
		return;
	}
	FinishClose();
}

void USOTMUpgradeStationWidget::HandleCloseAnimationFinished()
{
	UnbindAllFromAnimationFinished(NewAnimation);
	FinishClose();
}

void USOTMUpgradeStationWidget::FinishClose()
{
	bClosing = true;
	if (UWorld* World = GetWorld())
	{
		if (USOTMDemoPhase3WorldSubsystem* Phase3 = World->GetSubsystem<USOTMDemoPhase3WorldSubsystem>())
		{
			Phase3->CloseUpgradeUI();
		}
	}
}

bool USOTMUpgradeStationWidget::CanPurchase() const
{
	if (!PlayerState || PlayerState->IsSpeedBoostUnlocked())
	{
		return false;
	}
	const FSOTMObjectiveData Objective = ObjectiveState
		? ObjectiveState->GetCollectAllForestCoinsObjective() : FSOTMObjectiveData();
	return Objective.State == ESOTMObjectiveState::Completed &&
		PlayerState->GetAvailableCoins() >= GetDefault<USOTMPhase3Settings>()->SpeedBoostUnlockCost;
}

FText USOTMUpgradeStationWidget::GetRequirementText() const
{
	if (!PlayerState)
	{
		return FText::FromString(TEXT("PLAYER STATE UNAVAILABLE"));
	}
	if (PlayerState->IsSpeedBoostUnlocked())
	{
		return FText::FromString(TEXT("OWNED - READY TO USE"));
	}
	const FSOTMObjectiveData Objective = ObjectiveState
		? ObjectiveState->GetCollectAllForestCoinsObjective() : FSOTMObjectiveData();
	if (Objective.State != ESOTMObjectiveState::Completed)
	{
		return FText::FromString(TEXT("COLLECT ALL COINS FIRST"));
	}
	if (PlayerState->GetAvailableCoins() < GetDefault<USOTMPhase3Settings>()->SpeedBoostUnlockCost)
	{
		return FText::FromString(TEXT("NOT ENOUGH COINS"));
	}
	if (bHasAttemptedPurchase && LastPurchaseResult == ESOTMSpeedBoostPurchaseResult::SaveFailed)
	{
		return FText::FromString(TEXT("SAVE FAILED - NOTHING WAS DEDUCTED"));
	}
	return FText::FromString(TEXT("REQUIREMENTS COMPLETE"));
}

void USOTMUpgradeStationWidget::RefreshPresentation()
{
	// Text content, which ability is shown where, and button enabled state are all
	// driven from here; colors/layout stay fully Designer-controlled in WBP_UpgradeStation.
	//
	// Priority: Speed Boost is the priority ability until it's owned, then Lightning
	// Throw becomes the priority (it can't be bought before Speed Boost anyway - see
	// CanPurchaseLightning). The priority ability always fills the big widget group;
	// the other one is squeezed into the small "Lightning..."-named group, whichever
	// ability that actually is right now.
	const bool bSpeedOwned = PlayerState && PlayerState->IsSpeedBoostUnlocked();
	const bool bLightningOwned = PlayerState && PlayerState->IsLightningThrowUnlocked();
	const bool bPriorityIsLightning = bSpeedOwned;

	const USOTMPhase3Settings* Phase3Settings = GetDefault<USOTMPhase3Settings>();
	const USOTMLightningThrowSettings* LightningSettings = GetDefault<USOTMLightningThrowSettings>();

	// --- Priority (big) group -------------------------------------------------
	const bool bPriorityOwned = bPriorityIsLightning ? bLightningOwned : bSpeedOwned;
	const int32 Available = PlayerState ? PlayerState->GetAvailableCoins() : 0;
	const int32 PriorityCost = bPriorityIsLightning
		? LightningSettings->LightningThrowUnlockCost
		: Phase3Settings->SpeedBoostUnlockCost;
	const int32 PriorityLevel = bPriorityIsLightning
		? (bLightningOwned ? 1 : 0)
		: (PlayerState ? PlayerState->GetSpeedBoostLevel() : 0);

	if (AbilityNameText)
	{
		AbilityNameText->SetText(FText::FromString(bPriorityIsLightning ? TEXT("LIGHTNING THROW") : TEXT("SPEED BOOST")));
	}
	if (OwnershipText)
	{
		OwnershipText->SetText(FText::FromString(bPriorityOwned ? TEXT("OWNED") : TEXT("LOCKED")));
	}
	if (DescriptionText)
	{
		DescriptionText->SetText(FText::FromString(bPriorityIsLightning
			? TEXT("Throws a bolt that stuns nearby Cousins for a short time.\nPress [F] during Forest gameplay.")
			: TEXT("Temporarily increases movement speed.\nPress [Q] during Forest gameplay.")));
	}
	if (LevelText)
	{
		LevelText->SetText(FText::Format(FText::FromString(TEXT("LEVEL {0} / 1")), FText::AsNumber(PriorityLevel)));
	}
	if (SpeedIncreaseText)
	{
		SpeedIncreaseText->SetText(bPriorityIsLightning
			? FText::Format(FText::FromString(TEXT("RANGE   {0}")),
				FText::AsNumber(FMath::RoundToInt(LightningSettings->LightningThrowRange)))
			: FText::Format(FText::FromString(TEXT("SPEED INCREASE   +{0}%")),
				FText::AsNumber(FMath::RoundToInt((Phase3Settings->SpeedBoostMultiplier - 1.0f) * 100.0f))));
	}
	if (DurationText)
	{
		DurationText->SetText(bPriorityIsLightning
			? FText::Format(FText::FromString(TEXT("STUN DURATION   {0} SEC")), FText::AsNumber(LightningSettings->StunDuration))
			: FText::Format(FText::FromString(TEXT("DURATION   {0} SEC")), FText::AsNumber(Phase3Settings->SpeedBoostDuration)));
	}
	if (CooldownText)
	{
		CooldownText->SetText(FText::Format(FText::FromString(TEXT("COOLDOWN   {0} SEC")),
			FText::AsNumber(bPriorityIsLightning ? LightningSettings->LightningThrowCooldown : Phase3Settings->SpeedBoostCooldown)));
	}
	if (AvailableCoinsText)
	{
		AvailableCoinsText->SetText(FText::Format(FText::FromString(TEXT("YOUR COINS   {0}")), FText::AsNumber(Available)));
	}
	if (CostText)
	{
		CostText->SetText(FText::Format(FText::FromString(TEXT("UNLOCK COST   {0}")), FText::AsNumber(PriorityCost)));
	}
	if (RequirementText)
	{
		RequirementText->SetText(bPriorityIsLightning ? GetLightningRequirementText() : GetRequirementText());
	}
	if (UnlockButtonText)
	{
		UnlockButtonText->SetText(FText::FromString(bPriorityOwned
			? TEXT("OWNED")
			: (bPriorityIsLightning ? TEXT("UNLOCK LIGHTNING THROW") : TEXT("UNLOCK SPEED BOOST"))));
	}
	if (UnlockButton)
	{
		UnlockButton->SetIsEnabled(bPriorityIsLightning ? CanPurchaseLightning() : CanPurchase());
	}

	// --- Minimized (small) group ------------------------------------------------
	// Always the ability that ISN'T the priority right now: Lightning Throw before
	// Speed Boost is owned, Speed Boost itself afterward.
	const bool bSecondaryOwned = bPriorityIsLightning ? bSpeedOwned : bLightningOwned;
	if (LightningOwnershipText)
	{
		LightningOwnershipText->SetText(FText::Format(
			FText::FromString(TEXT("{0}   {1}")),
			FText::FromString(bPriorityIsLightning ? TEXT("SPEED BOOST") : TEXT("LIGHTNING THROW")),
			FText::FromString(bSecondaryOwned ? TEXT("OWNED") : TEXT("LOCKED"))));
	}
	if (LightningRequirementText)
	{
		LightningRequirementText->SetText(bPriorityIsLightning ? GetRequirementText() : GetLightningRequirementText());
	}
	if (LightningUnlockButtonText)
	{
		LightningUnlockButtonText->SetText(FText::FromString(bSecondaryOwned
			? TEXT("OWNED")
			: (bPriorityIsLightning ? TEXT("UNLOCK SPEED BOOST") : TEXT("UNLOCK LIGHTNING THROW"))));
	}
	if (LightningUnlockButton)
	{
		LightningUnlockButton->SetIsEnabled(bPriorityIsLightning ? CanPurchase() : CanPurchaseLightning());
	}
}
