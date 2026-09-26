#include "UI/SOTMSkillTreeWidget.h"

#include "Ability/SOTMLightningThrowSettings.h"
#include "Ability/SOTMPhase3Settings.h"
#include "Ability/SOTMSkillTreeSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Demo/SOTMDemoPhase3WorldSubsystem.h"
#include "Engine/GameInstance.h"
#include "InputCoreTypes.h"
#include "SOTMPlayerStateSubsystem.h"

namespace SOTMSkillTreePrivate
{
	const FLinearColor LockedColor(0.12f, 0.12f, 0.12f, 1.0f);
	const FLinearColor UnlockableColor(0.10f, 0.35f, 0.55f, 1.0f);
	const FLinearColor NotEnoughPointsColor(0.35f, 0.28f, 0.10f, 1.0f);
	const FLinearColor UnlockedColor(0.15f, 0.45f, 0.20f, 1.0f);
}

void USOTMSkillTreeWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	if (!WidgetTree || !WidgetTree->RootWidget)
	{
		// Nothing bound a root widget, so no WBP subclass laid this screen out by hand
		// (see the class comment) - build the fixed layout entirely in C++ instead.
		BuildDefaultLayout();
	}

	// Wire up the 4 fixed nodes' buttons (C++-built or WBP-placed, either way) and the
	// close/confirm buttons. AddUniqueDynamic makes re-running this a no-op, and every
	// widget is checked for null since any of them may simply not exist (e.g. an
	// in-progress WBP override that hasn't placed them all yet).
	if (SpeedBoostNode2Button)
	{
		SpeedBoostNode2Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSpeedBoostNode2Clicked);
	}
	if (SpeedBoostNode3Button)
	{
		SpeedBoostNode3Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSpeedBoostNode3Clicked);
	}
	if (LightningThrowNode2Button)
	{
		LightningThrowNode2Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightningThrowNode2Clicked);
	}
	if (LightningThrowNode3Button)
	{
		LightningThrowNode3Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightningThrowNode3Clicked);
	}
	if (SpeedBoostNode4Button)
	{
		SpeedBoostNode4Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSpeedBoostNode4Clicked);
	}
	if (LightningThrowNode4Button)
	{
		LightningThrowNode4Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLightningThrowNode4Clicked);
	}
	if (CloseButton)
	{
		CloseButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmUpgradeClicked);
	}
	if (ConfirmCancelButton)
	{
		ConfirmCancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelUpgradeClicked);
	}
}

void USOTMSkillTreeWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	PlayerState = GetGameInstance() ? GetGameInstance()->GetSubsystem<USOTMPlayerStateSubsystem>() : nullptr;
	RefreshAllNodes();
	SetKeyboardFocus();
}

FReply USOTMSkillTreeWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (ConfirmPanel && ConfirmPanel->GetVisibility() != ESlateVisibility::Collapsed)
		{
			// Cancel the pending purchase decision first - Escape shouldn't skip past it
			// and close the whole tree while a confirmation is up.
			HandleCancelUpgradeClicked();
		}
		else
		{
			HandleCloseClicked();
		}
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void USOTMSkillTreeWidget::HandleCloseClicked()
{
	if (UWorld* World = GetWorld())
	{
		if (USOTMDemoPhase3WorldSubsystem* Phase3 = World->GetSubsystem<USOTMDemoPhase3WorldSubsystem>())
		{
			Phase3->CloseSkillTreeUI();
			return;
		}
	}
	RemoveFromParent();
}

void USOTMSkillTreeWidget::HandleSpeedBoostNode2Clicked()
{
	HandleNodeClicked(TEXT("SpeedBoost"), 2);
}

void USOTMSkillTreeWidget::HandleSpeedBoostNode3Clicked()
{
	HandleNodeClicked(TEXT("SpeedBoost"), 3);
}

void USOTMSkillTreeWidget::HandleLightningThrowNode2Clicked()
{
	HandleNodeClicked(TEXT("LightningThrow"), 2);
}

void USOTMSkillTreeWidget::HandleLightningThrowNode3Clicked()
{
	HandleNodeClicked(TEXT("LightningThrow"), 3);
}

void USOTMSkillTreeWidget::HandleSpeedBoostNode4Clicked()
{
	HandleNodeClicked(TEXT("SpeedBoost"), 4);
}

void USOTMSkillTreeWidget::HandleLightningThrowNode4Clicked()
{
	HandleNodeClicked(TEXT("LightningThrow"), 4);
}

int32 USOTMSkillTreeWidget::GetAbilityPoints() const
{
	return PlayerState ? PlayerState->GetAbilityPoints() : 0;
}

bool USOTMSkillTreeWidget::IsAbilityBaseUnlocked(const FName AbilityId) const
{
	if (!PlayerState)
	{
		return false;
	}
	// The one spot that has to know each concrete ability by name, because PlayerState's
	// base-unlock query is still per-ability (IsSpeedBoostUnlocked/IsLightningThrowUnlocked)
	// rather than a generic FName lookup. Add a line here when a third ability is added.
	if (AbilityId == TEXT("SpeedBoost"))
	{
		return PlayerState->IsSpeedBoostUnlocked();
	}
	if (AbilityId == TEXT("LightningThrow"))
	{
		return PlayerState->IsLightningThrowUnlocked();
	}
	return false;
}

bool USOTMSkillTreeWidget::AreBothAbilitiesUnlocked() const
{
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	if (!Settings || Settings->Abilities.Num() == 0)
	{
		return false;
	}
	// Generic over every ability USOTMSkillTreeSettings defines - currently that's
	// exactly two (Speed Boost, Lightning Throw), i.e. "both", but this reads correctly
	// if a third is ever added.
	for (const FSOTMSkillTreeAbilityDefinition& Ability : Settings->Abilities)
	{
		if (!IsAbilityBaseUnlocked(Ability.AbilityId))
		{
			return false;
		}
	}
	return true;
}

int32 USOTMSkillTreeWidget::GetAbilityCurrentLevel(const FName AbilityId) const
{
	if (!IsAbilityBaseUnlocked(AbilityId))
	{
		return 0;
	}
	return 1 + (PlayerState ? PlayerState->GetSkillUpgradeLevel(AbilityId) : 0);
}

ESOTMSkillNodeState USOTMSkillTreeWidget::GetNodeState(const FName AbilityId, const int32 Level) const
{
	if (Level <= 1)
	{
		// Full afford/prerequisite preview for the coin-paid base unlock already lives in
		// USOTMUpgradeStationWidget; this screen just reflects owned-or-not. (No Level 1
		// node exists in this screen's fixed layout - only the shared root banner - but
		// this branch is kept for Blueprint/future callers.)
		return IsAbilityBaseUnlocked(AbilityId) ? ESOTMSkillNodeState::Unlocked : ESOTMSkillNodeState::Unlockable;
	}
	if (!AreBothAbilitiesUnlocked())
	{
		// Every ability must be owned before ANY upgrade node opens up, even if this
		// particular ability's own base is already owned.
		return ESOTMSkillNodeState::Locked;
	}
	const int32 CurrentLevel = GetAbilityCurrentLevel(AbilityId);
	if (CurrentLevel >= Level)
	{
		return ESOTMSkillNodeState::Unlocked;
	}
	if (CurrentLevel != Level - 1)
	{
		// Base ability not owned yet (CurrentLevel is 0), or the previous level for this
		// ability hasn't been bought yet.
		return ESOTMSkillNodeState::Locked;
	}
	const int32 Cost = GetNodeCost(AbilityId, Level);
	return GetAbilityPoints() >= Cost ? ESOTMSkillNodeState::Unlockable : ESOTMSkillNodeState::NotEnoughPoints;
}

int32 USOTMSkillTreeWidget::GetNodeCost(const FName AbilityId, const int32 Level) const
{
	if (Level <= 1)
	{
		if (AbilityId == TEXT("SpeedBoost"))
		{
			return GetDefault<USOTMPhase3Settings>()->SpeedBoostUnlockCost;
		}
		if (AbilityId == TEXT("LightningThrow"))
		{
			return GetDefault<USOTMLightningThrowSettings>()->LightningThrowUnlockCost;
		}
		return 0;
	}
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(AbilityId, Level) : nullptr;
	return LevelDef ? LevelDef->AbilityPointCost : 0;
}

FText USOTMSkillTreeWidget::GetNodeDisplayName(const FName AbilityId, const int32 Level) const
{
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	if (Level <= 1)
	{
		// Root node of this ability's tree: deliberately generic (not the ability's own
		// name) since the real, named tree of upgrades only starts at Level 2 - this is
		// just the coin-paid gate that opens it.
		return FText::FromString(TEXT("UNLOCK ABILITY"));
	}
	const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(AbilityId, Level) : nullptr;
	return LevelDef ? LevelDef->DisplayName : FText::Format(FText::FromString(TEXT("LEVEL {0}")), Level);
}

FText USOTMSkillTreeWidget::GetNodeDescription(const FName AbilityId, const int32 Level) const
{
	if (Level <= 1)
	{
		return FText::FromString(TEXT("Unlock this ability (paid in coins) to open its skill tree."));
	}
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(AbilityId, Level) : nullptr;
	return LevelDef ? LevelDef->Description : FText::GetEmpty();
}

bool USOTMSkillTreeWidget::TryUnlockNode(const FName AbilityId, const int32 Level)
{
	bool bSuccess = false;
	if (Level <= 1)
	{
		if (UWorld* World = GetWorld())
		{
			if (USOTMDemoPhase3WorldSubsystem* Phase3 = World->GetSubsystem<USOTMDemoPhase3WorldSubsystem>())
			{
				if (AbilityId == TEXT("SpeedBoost"))
				{
					bSuccess = Phase3->TryPurchaseSpeedBoost() == ESOTMSpeedBoostPurchaseResult::Success;
				}
				else if (AbilityId == TEXT("LightningThrow"))
				{
					bSuccess = Phase3->TryPurchaseLightningThrow() == ESOTMLightningThrowPurchaseResult::Success;
				}
			}
		}
	}
	else if (PlayerState)
	{
		bSuccess = PlayerState->TryUnlockSkillUpgrade(AbilityId, Level, GetNodeCost(AbilityId, Level));
	}
	RefreshAllNodes();
	return bSuccess;
}

void USOTMSkillTreeWidget::RefreshAllNodes()
{
	if (PointsText)
	{
		PointsText->SetText(FText::Format(FText::FromString(TEXT("ABILITY POINTS   {0}")), FText::AsNumber(GetAbilityPoints())));
	}
	if (RootBannerText)
	{
		const bool bBothUnlocked = AreBothAbilitiesUnlocked();
		RootBannerText->SetText(bBothUnlocked
			? FText::FromString(TEXT("UNLOCK ABILITY   -   BOTH ABILITIES OWNED, UPGRADES UNLOCKED"))
			: FText::FromString(TEXT("UNLOCK ABILITY   -   OWN BOTH ABILITIES TO UNLOCK UPGRADES")));
		RootBannerText->SetColorAndOpacity(bBothUnlocked
			? FSlateColor(FLinearColor(0.55f, 0.95f, 0.55f, 1.0f))
			: FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 0.85f)));
	}

	// The 6 fixed nodes - explicit, no loop (see the class comment).
	RefreshNodeVisual(SpeedBoostNode2Button, SpeedBoostNode2Text, TEXT("SpeedBoost"), 2);
	RefreshNodeVisual(SpeedBoostNode3Button, SpeedBoostNode3Text, TEXT("SpeedBoost"), 3);
	RefreshNodeVisual(SpeedBoostNode4Button, SpeedBoostNode4Text, TEXT("SpeedBoost"), 4);
	RefreshNodeVisual(LightningThrowNode2Button, LightningThrowNode2Text, TEXT("LightningThrow"), 2);
	RefreshNodeVisual(LightningThrowNode3Button, LightningThrowNode3Text, TEXT("LightningThrow"), 3);
	RefreshNodeVisual(LightningThrowNode4Button, LightningThrowNode4Text, TEXT("LightningThrow"), 4);
}

void USOTMSkillTreeWidget::RefreshNodeVisual(UButton* Button, UTextBlock* Text, const FName AbilityId, const int32 Level) const
{
	if (!Button || !Text)
	{
		return;
	}
	const ESOTMSkillNodeState State = GetNodeState(AbilityId, Level);
	const int32 Cost = GetNodeCost(AbilityId, Level);
	const FText DisplayName = GetNodeDisplayName(AbilityId, Level);

	FLinearColor Color = SOTMSkillTreePrivate::LockedColor;
	FText Status;
	bool bEnabled = false;
	switch (State)
	{
	case ESOTMSkillNodeState::Unlocked:
		Color = SOTMSkillTreePrivate::UnlockedColor;
		Status = FText::FromString(TEXT("OWNED"));
		bEnabled = false;
		break;
	case ESOTMSkillNodeState::Unlockable:
		Color = SOTMSkillTreePrivate::UnlockableColor;
		Status = FText::Format(FText::FromString(TEXT("COST {0}")), FText::AsNumber(Cost));
		bEnabled = true;
		break;
	case ESOTMSkillNodeState::NotEnoughPoints:
		Color = SOTMSkillTreePrivate::NotEnoughPointsColor;
		Status = FText::Format(FText::FromString(TEXT("NEED {0} POINTS")), FText::AsNumber(Cost));
		bEnabled = false;
		break;
	case ESOTMSkillNodeState::Locked:
	default:
		Color = SOTMSkillTreePrivate::LockedColor;
		Status = FText::FromString(TEXT("LOCKED"));
		bEnabled = false;
		break;
	}

	Text->SetText(FText::Format(FText::FromString(TEXT("{0}\n{1}")), DisplayName, Status));
	Button->SetBackgroundColor(Color);
	Button->SetIsEnabled(bEnabled);
}

void USOTMSkillTreeWidget::HandleNodeClicked(const FName AbilityId, const int32 Level)
{
	if (Level <= 1)
	{
		// Base unlock: nothing to compare (unlocking from nothing), so it still buys
		// immediately, same as before.
		TryUnlockNode(AbilityId, Level);
		return;
	}
	OpenUpgradeConfirm(AbilityId, Level);
}

void USOTMSkillTreeWidget::OpenUpgradeConfirm(const FName AbilityId, const int32 Level)
{
	if (GetNodeState(AbilityId, Level) != ESOTMSkillNodeState::Unlockable)
	{
		// Not actually buyable right now (already owned, locked, or can't afford it) -
		// the node button is disabled in every other state, but AP/level could have
		// changed between the click firing and this running, so re-check here too.
		return;
	}
	PendingAbilityId = AbilityId;
	PendingLevel = Level;
	if (ConfirmTitleText)
	{
		ConfirmTitleText->SetText(GetNodeDisplayName(AbilityId, Level));
	}
	if (ConfirmComparisonText)
	{
		ConfirmComparisonText->SetText(GetNodeComparisonText(AbilityId, Level));
	}
	if (ConfirmCostText)
	{
		ConfirmCostText->SetText(FText::Format(
			FText::FromString(TEXT("COST   {0} ABILITY POINT(S)")), FText::AsNumber(GetNodeCost(AbilityId, Level))));
	}
	if (ConfirmDimBackground)
	{
		ConfirmDimBackground->SetVisibility(ESlateVisibility::Visible);
	}
	if (ConfirmPanel)
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void USOTMSkillTreeWidget::CloseUpgradeConfirm()
{
	PendingAbilityId = NAME_None;
	PendingLevel = 0;
	if (ConfirmDimBackground)
	{
		ConfirmDimBackground->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ConfirmPanel)
	{
		ConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMSkillTreeWidget::HandleConfirmUpgradeClicked()
{
	// TryUnlockNode does the actual spend + RefreshAllNodes() (updates every node's
	// color/status and the AP counter), so the tree is already showing the new state by
	// the time this panel closes.
	TryUnlockNode(PendingAbilityId, PendingLevel);
	CloseUpgradeConfirm();
}

void USOTMSkillTreeWidget::HandleCancelUpgradeClicked()
{
	CloseUpgradeConfirm();
}

FText USOTMSkillTreeWidget::GetNodeComparisonText(const FName AbilityId, const int32 Level) const
{
	if (!PlayerState || Level < 2)
	{
		return FText::GetEmpty();
	}
	const USOTMSkillTreeSettings* Settings = GetDefault<USOTMSkillTreeSettings>();
	const FSOTMSkillTreeLevelDefinition* LevelDef = Settings ? Settings->FindLevel(AbilityId, Level) : nullptr;
	if (!LevelDef)
	{
		return FText::GetEmpty();
	}

	// "Before" is the ability's CURRENT effective stat (base + every level already
	// bought); "after" is that plus this specific level's own delta - the purchase
	// hasn't happened yet, so this previews it rather than reading it back post-purchase.
	TArray<FString> Lines;
	if (AbilityId == TEXT("SpeedBoost"))
	{
		const float BeforeDuration = PlayerState->GetEffectiveSpeedBoostDuration();
		const float AfterDuration = BeforeDuration + LevelDef->DurationDeltaSeconds;
		const float BeforeCooldown = PlayerState->GetEffectiveSpeedBoostCooldown();
		const float AfterCooldown = FMath::Max(0.1f, BeforeCooldown + LevelDef->CooldownDeltaSeconds);
		if (!FMath::IsNearlyZero(LevelDef->DurationDeltaSeconds))
		{
			Lines.Add(FString::Printf(TEXT("DURATION      %.1fs   ->   %.1fs"), BeforeDuration, AfterDuration));
		}
		if (!FMath::IsNearlyZero(LevelDef->CooldownDeltaSeconds))
		{
			Lines.Add(FString::Printf(TEXT("COOLDOWN      %.1fs   ->   %.1fs"), BeforeCooldown, AfterCooldown));
		}
	}
	else if (AbilityId == TEXT("LightningThrow"))
	{
		const float BeforeCooldown = PlayerState->GetEffectiveLightningThrowCooldown();
		const float AfterCooldown = FMath::Max(0.1f, BeforeCooldown + LevelDef->CooldownDeltaSeconds);
		const float BeforeRange = PlayerState->GetEffectiveLightningThrowRange();
		const float AfterRange = BeforeRange + LevelDef->RangeDelta;
		if (!FMath::IsNearlyZero(LevelDef->CooldownDeltaSeconds))
		{
			Lines.Add(FString::Printf(TEXT("COOLDOWN      %.1fs   ->   %.1fs"), BeforeCooldown, AfterCooldown));
		}
		if (!FMath::IsNearlyZero(LevelDef->RangeDelta))
		{
			Lines.Add(FString::Printf(TEXT("RANGE      %.0fm   ->   %.0fm"), BeforeRange / 100.0f, AfterRange / 100.0f));
		}
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

UButton* USOTMSkillTreeWidget::BuildNodeButton(const FName ButtonWidgetName, const FName TextWidgetName, TObjectPtr<UTextBlock>& OutText)
{
	UButton* NodeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), ButtonWidgetName);
	NodeButton->SetBackgroundColor(SOTMSkillTreePrivate::LockedColor);

	OutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TextWidgetName);
	OutText->SetJustification(ETextJustify::Center);
	OutText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = OutText->GetFont();
		Font.Size = 13;
		OutText->SetFont(Font);
	}
	NodeButton->AddChild(OutText);

	return NodeButton;
}

void USOTMSkillTreeWidget::BuildDefaultLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transactional);
	}

	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("RootOverlay"));
	WidgetTree->RootWidget = RootOverlay;

	// --- Backdrop ---------------------------------------------------------------
	UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BackdropBorder"));
	Backdrop->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.03f, 0.92f));
	if (UOverlaySlot* BackdropSlot = RootOverlay->AddChildToOverlay(Backdrop))
	{
		BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
		BackdropSlot->SetVerticalAlignment(VAlign_Fill);
	}

	// --- Main screen content: header row / root banner / the fixed node tree -----
	UVerticalBox* Main = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MainBox"));
	if (UOverlaySlot* MainSlot = RootOverlay->AddChildToOverlay(Main))
	{
		MainSlot->SetHorizontalAlignment(HAlign_Fill);
		MainSlot->SetVerticalAlignment(VAlign_Fill);
	}

	UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("HeaderRow"));
	if (UVerticalBoxSlot* HeaderRowSlot = Main->AddChildToVerticalBox(HeaderRow))
	{
		HeaderRowSlot->SetPadding(FMargin(24.0f, 16.0f, 24.0f, 8.0f));
	}

	PointsText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PointsText"));
	PointsText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.5f, 1.0f)));
	{
		FSlateFontInfo Font = PointsText->GetFont();
		Font.Size = 20;
		PointsText->SetFont(Font);
	}
	if (UHorizontalBoxSlot* PointsSlot = HeaderRow->AddChildToHorizontalBox(PointsText))
	{
		PointsSlot->SetVerticalAlignment(VAlign_Center);
	}

	CloseButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CloseButton"));
	UTextBlock* CloseLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CloseLabel"));
	CloseLabel->SetText(FText::FromString(TEXT("CLOSE")));
	CloseLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CloseButton->AddChild(CloseLabel);
	if (UHorizontalBoxSlot* CloseSlot = HeaderRow->AddChildToHorizontalBox(CloseButton))
	{
		CloseSlot->SetVerticalAlignment(VAlign_Center);
		CloseSlot->SetPadding(FMargin(48.0f, 0.0f, 0.0f, 0.0f));
	}

	RootBannerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("RootBannerText"));
	RootBannerText->SetJustification(ETextJustify::Center);
	{
		FSlateFontInfo Font = RootBannerText->GetFont();
		Font.Size = 16;
		RootBannerText->SetFont(Font);
	}
	if (UVerticalBoxSlot* BannerSlot = Main->AddChildToVerticalBox(RootBannerText))
	{
		BannerSlot->SetHorizontalAlignment(HAlign_Center);
		BannerSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	// --- Fixed node tree: 2 columns, each stacking Level 2, then 3, then 4 --------
	//
	//              [ UNLOCK ABILITY ]        <- RootBannerText, above
	//             /                  \
	//   SPEED BOOST Lv2      LIGHTNING THROW Lv2
	//         |                       |
	//   SPEED BOOST Lv3      LIGHTNING THROW Lv3
	//         |                       |
	//   SPEED BOOST Lv4      LIGHTNING THROW Lv4
	//
	UHorizontalBox* ColumnsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ColumnsRow"));
	if (UVerticalBoxSlot* ColumnsSlot = Main->AddChildToVerticalBox(ColumnsRow))
	{
		ColumnsSlot->SetHorizontalAlignment(HAlign_Center);
		ColumnsSlot->SetPadding(FMargin(24.0f));
	}

	// Speed Boost column.
	UVerticalBox* SpeedColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SpeedBoostColumn"));
	if (UHorizontalBoxSlot* SpeedColumnSlot = ColumnsRow->AddChildToHorizontalBox(SpeedColumn))
	{
		SpeedColumnSlot->SetPadding(FMargin(24.0f, 0.0f));
	}

	UButton* SpeedNode2 = BuildNodeButton(TEXT("SpeedBoostNode2Button"), TEXT("SpeedBoostNode2Text"), SpeedBoostNode2Text);
	SpeedBoostNode2Button = SpeedNode2;
	USizeBox* SpeedNode2Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SpeedBoostNode2Size"));
	SpeedNode2Size->SetWidthOverride(220.0f);
	SpeedNode2Size->SetHeightOverride(64.0f);
	SpeedNode2Size->AddChild(SpeedNode2);
	if (UVerticalBoxSlot* SpeedNode2Slot = SpeedColumn->AddChildToVerticalBox(SpeedNode2Size))
	{
		SpeedNode2Slot->SetHorizontalAlignment(HAlign_Center);
		SpeedNode2Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UButton* SpeedNode3 = BuildNodeButton(TEXT("SpeedBoostNode3Button"), TEXT("SpeedBoostNode3Text"), SpeedBoostNode3Text);
	SpeedBoostNode3Button = SpeedNode3;
	USizeBox* SpeedNode3Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SpeedBoostNode3Size"));
	SpeedNode3Size->SetWidthOverride(220.0f);
	SpeedNode3Size->SetHeightOverride(64.0f);
	SpeedNode3Size->AddChild(SpeedNode3);
	if (UVerticalBoxSlot* SpeedNode3Slot = SpeedColumn->AddChildToVerticalBox(SpeedNode3Size))
	{
		SpeedNode3Slot->SetHorizontalAlignment(HAlign_Center);
		SpeedNode3Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UButton* SpeedNode4 = BuildNodeButton(TEXT("SpeedBoostNode4Button"), TEXT("SpeedBoostNode4Text"), SpeedBoostNode4Text);
	SpeedBoostNode4Button = SpeedNode4;
	USizeBox* SpeedNode4Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SpeedBoostNode4Size"));
	SpeedNode4Size->SetWidthOverride(220.0f);
	SpeedNode4Size->SetHeightOverride(64.0f);
	SpeedNode4Size->AddChild(SpeedNode4);
	if (UVerticalBoxSlot* SpeedNode4Slot = SpeedColumn->AddChildToVerticalBox(SpeedNode4Size))
	{
		SpeedNode4Slot->SetHorizontalAlignment(HAlign_Center);
	}

	// Lightning Throw column.
	UVerticalBox* LightningColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LightningThrowColumn"));
	if (UHorizontalBoxSlot* LightningColumnSlot = ColumnsRow->AddChildToHorizontalBox(LightningColumn))
	{
		LightningColumnSlot->SetPadding(FMargin(24.0f, 0.0f));
	}

	UButton* LightningNode2 = BuildNodeButton(TEXT("LightningThrowNode2Button"), TEXT("LightningThrowNode2Text"), LightningThrowNode2Text);
	LightningThrowNode2Button = LightningNode2;
	USizeBox* LightningNode2Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LightningThrowNode2Size"));
	LightningNode2Size->SetWidthOverride(220.0f);
	LightningNode2Size->SetHeightOverride(64.0f);
	LightningNode2Size->AddChild(LightningNode2);
	if (UVerticalBoxSlot* LightningNode2Slot = LightningColumn->AddChildToVerticalBox(LightningNode2Size))
	{
		LightningNode2Slot->SetHorizontalAlignment(HAlign_Center);
		LightningNode2Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UButton* LightningNode3 = BuildNodeButton(TEXT("LightningThrowNode3Button"), TEXT("LightningThrowNode3Text"), LightningThrowNode3Text);
	LightningThrowNode3Button = LightningNode3;
	USizeBox* LightningNode3Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LightningThrowNode3Size"));
	LightningNode3Size->SetWidthOverride(220.0f);
	LightningNode3Size->SetHeightOverride(64.0f);
	LightningNode3Size->AddChild(LightningNode3);
	if (UVerticalBoxSlot* LightningNode3Slot = LightningColumn->AddChildToVerticalBox(LightningNode3Size))
	{
		LightningNode3Slot->SetHorizontalAlignment(HAlign_Center);
		LightningNode3Slot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	}

	UButton* LightningNode4 = BuildNodeButton(TEXT("LightningThrowNode4Button"), TEXT("LightningThrowNode4Text"), LightningThrowNode4Text);
	LightningThrowNode4Button = LightningNode4;
	USizeBox* LightningNode4Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("LightningThrowNode4Size"));
	LightningNode4Size->SetWidthOverride(220.0f);
	LightningNode4Size->SetHeightOverride(64.0f);
	LightningNode4Size->AddChild(LightningNode4);
	if (UVerticalBoxSlot* LightningNode4Slot = LightningColumn->AddChildToVerticalBox(LightningNode4Size))
	{
		LightningNode4Slot->SetHorizontalAlignment(HAlign_Center);
	}

	// --- Upgrade confirmation popup, on top of everything above ------------------
	ConfirmDimBackground = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmDimBackground"));
	ConfirmDimBackground->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));
	ConfirmDimBackground->SetHorizontalAlignment(HAlign_Center);
	ConfirmDimBackground->SetVerticalAlignment(VAlign_Center);
	ConfirmDimBackground->SetVisibility(ESlateVisibility::Collapsed);
	if (UOverlaySlot* DimSlot = RootOverlay->AddChildToOverlay(ConfirmDimBackground))
	{
		DimSlot->SetHorizontalAlignment(HAlign_Fill);
		DimSlot->SetVerticalAlignment(VAlign_Fill);
	}

	ConfirmPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ConfirmPanel"));
	ConfirmPanel->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.0f));
	ConfirmPanel->SetPadding(FMargin(24.0f));
	ConfirmDimBackground->AddChild(ConfirmPanel);

	UVerticalBox* ConfirmBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ConfirmBox"));
	ConfirmPanel->AddChild(ConfirmBox);

	ConfirmTitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmTitleText"));
	ConfirmTitleText->SetJustification(ETextJustify::Center);
	ConfirmTitleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	{
		FSlateFontInfo Font = ConfirmTitleText->GetFont();
		Font.Size = 18;
		ConfirmTitleText->SetFont(Font);
	}
	if (UVerticalBoxSlot* ConfirmTitleSlot = ConfirmBox->AddChildToVerticalBox(ConfirmTitleText))
	{
		ConfirmTitleSlot->SetHorizontalAlignment(HAlign_Center);
		ConfirmTitleSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	ConfirmComparisonText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmComparisonText"));
	ConfirmComparisonText->SetJustification(ETextJustify::Center);
	ConfirmComparisonText->SetColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)));
	if (UVerticalBoxSlot* ComparisonSlot = ConfirmBox->AddChildToVerticalBox(ConfirmComparisonText))
	{
		ComparisonSlot->SetHorizontalAlignment(HAlign_Center);
		ComparisonSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 12.0f));
	}

	ConfirmCostText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmCostText"));
	ConfirmCostText->SetJustification(ETextJustify::Center);
	ConfirmCostText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.5f, 1.0f)));
	if (UVerticalBoxSlot* CostSlot = ConfirmBox->AddChildToVerticalBox(ConfirmCostText))
	{
		CostSlot->SetHorizontalAlignment(HAlign_Center);
		CostSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 16.0f));
	}

	UHorizontalBox* ConfirmButtonsRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("ConfirmButtonsRow"));
	if (UVerticalBoxSlot* ButtonsRowSlot = ConfirmBox->AddChildToVerticalBox(ConfirmButtonsRow))
	{
		ButtonsRowSlot->SetHorizontalAlignment(HAlign_Center);
	}

	ConfirmButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmButton"));
	UTextBlock* ConfirmLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConfirmLabel"));
	ConfirmLabel->SetText(FText::FromString(TEXT("CONFIRM")));
	ConfirmLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmButton->AddChild(ConfirmLabel);
	if (UHorizontalBoxSlot* ConfirmButtonSlot = ConfirmButtonsRow->AddChildToHorizontalBox(ConfirmButton))
	{
		ConfirmButtonSlot->SetPadding(FMargin(0.0f, 0.0f, 16.0f, 0.0f));
	}

	ConfirmCancelButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ConfirmCancelButton"));
	UTextBlock* CancelLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CancelLabel"));
	CancelLabel->SetText(FText::FromString(TEXT("CANCEL")));
	CancelLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ConfirmCancelButton->AddChild(CancelLabel);
	ConfirmButtonsRow->AddChildToHorizontalBox(ConfirmCancelButton);
}
