#include "UI/SOTMIngameUIWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Demo/SOTMDemoPhase2WorldSubsystem.h"
#include "Demo/SOTMDemoPhase3WorldSubsystem.h"
#include "Ability/SOTMLightningThrowWorldSubsystem.h"
#include "Ability/SOTMLightningThrowSettings.h"
#include "Ability/SOTMPhase3Settings.h"
#include "Demo/SOTMDemoPhase4WorldSubsystem.h"
#include "Gate/SOTMKeyGateActor.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "SOTMPlayerBlueprintLibrary.h"
#include "Styling/SlateBrush.h"
#include "TimerManager.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMHUD, Log, All);

namespace
{
	// --- SOTM Horror HUD palette --------------------------------------------
	// Oxidised, low-saturation tones instead of flat/bright fills, so panels
	// read like they're lit by a single dying candle rather than a UI kit.
	const FLinearColor PanelColor(0.014f, 0.012f, 0.013f, 0.90f);
	const FLinearColor PrimaryTextColor(0.90f, 0.87f, 0.80f, 1.0f);
	const FLinearColor MutedTextColor(0.42f, 0.38f, 0.40f, 1.0f);
	const FLinearColor PurpleAccent(0.52f, 0.22f, 0.62f, 1.0f);
	const FLinearColor GoldAccent(0.82f, 0.62f, 0.28f, 1.0f);
	const FLinearColor RedAccent(0.78f, 0.05f, 0.07f, 1.0f);
	const FLinearColor BloodOutline(0.42f, 0.08f, 0.09f, 0.65f);

	UTextBlock* CreateText(
		UWidgetTree* Tree,
		const FName Name,
		const FText& Text,
		const int32 Size,
		const FLinearColor& Color,
		const int32 LetterSpacing = 0)
	{
		UTextBlock* Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Widget->SetText(Text);
		Widget->SetColorAndOpacity(FSlateColor(Color));
		Widget->SetShadowOffset(FVector2D(1.0f, 2.0f));
		Widget->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.95f));
		FSlateFontInfo Font = Widget->GetFont();
		Font.Size = Size;
		Font.LetterSpacing = LetterSpacing;
		Widget->SetFont(Font);
		return Widget;
	}

	void AddVertical(UVerticalBox* Parent, UWidget* Child, const FMargin& Padding)
	{
		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}
	}

	// --- Objective panel palette ---------------------------------------------
	// Still used at runtime by SetMissionTask() (dynamic row color) and by the
	// Refresh*/Handle* functions below that recolor text on state changes.
	// Panel construction itself now lives entirely in WBP_InGameMain.
	const FLinearColor ObjectiveTitleColor(0.86f, 0.10f, 0.10f, 1.0f);
	const FLinearColor ObjectiveBodyColor(0.94f, 0.92f, 0.90f, 1.0f);
	const FLinearColor ObjectiveAccentColor(0.80f, 0.16f, 0.14f, 1.0f);
	const FLinearColor ObjectiveMutedColor(0.68f, 0.60f, 0.58f, 1.0f);

#if !UE_BUILD_SHIPPING
	TWeakObjectPtr<USOTMIngameUIWidget> ActiveDevelopmentHUD;
	bool bPendingObjectivePreview = false;
	bool bPendingUpgradePreview = false;
	bool bPendingBossPreview = false;
	FIntPoint PendingPreviewCaptureResolution = FIntPoint::ZeroValue;

	FAutoConsoleCommand TestObjectiveCommand(
		TEXT("SOTM.HUD.TestObjective"),
		TEXT("Show development-only Objective Panel preview data."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			bPendingObjectivePreview = true;
			if (USOTMIngameUIWidget* HUD = ActiveDevelopmentHUD.Get())
			{
				HUD->ShowDevelopmentObjectivePreview();
				bPendingObjectivePreview = false;
			}
		}));

	FAutoConsoleCommand TestUpgradeCommand(
		TEXT("SOTM.HUD.TestUpgradeProgress"),
		TEXT("Show development-only upgrade requirement/progress preview data."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			bPendingUpgradePreview = true;
			if (USOTMIngameUIWidget* HUD = ActiveDevelopmentHUD.Get())
			{
				HUD->ShowDevelopmentUpgradePreview();
				bPendingUpgradePreview = false;
			}
		}));

	FAutoConsoleCommand TestBossCommand(
		TEXT("SOTM.HUD.TestBossProgress"),
		TEXT("Show development-only Boss Progress preview data."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			bPendingBossPreview = true;
			if (USOTMIngameUIWidget* HUD = ActiveDevelopmentHUD.Get())
			{
				HUD->ShowDevelopmentBossPreview();
				bPendingBossPreview = false;
			}
		}));

	FAutoConsoleCommand ClearPreviewCommand(
		TEXT("SOTM.HUD.ClearPreview"),
		TEXT("Clear all development-only HUD preview data."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			bPendingObjectivePreview = false;
			bPendingUpgradePreview = false;
			bPendingBossPreview = false;
			if (USOTMIngameUIWidget* HUD = ActiveDevelopmentHUD.Get())
			{
				HUD->ClearDevelopmentPreview();
			}
		}));

	FAutoConsoleCommand CapturePreview1080Command(
		TEXT("SOTM.HUD.CapturePreview1080"),
		TEXT("Show all development HUD preview data and capture it at 1920x1080."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			PendingPreviewCaptureResolution = FIntPoint(1920, 1080);
			if (USOTMIngameUIWidget* HUD = ActiveDevelopmentHUD.Get())
			{
				HUD->CaptureDevelopmentPreview(1920, 1080);
				PendingPreviewCaptureResolution = FIntPoint::ZeroValue;
			}
		}));

	FAutoConsoleCommand CapturePreview720Command(
		TEXT("SOTM.HUD.CapturePreview720"),
		TEXT("Show all development HUD preview data and capture it at 1280x720."),
		FConsoleCommandDelegate::CreateLambda([]
		{
			PendingPreviewCaptureResolution = FIntPoint(1280, 720);
			if (USOTMIngameUIWidget* HUD = ActiveDevelopmentHUD.Get())
			{
				HUD->CaptureDevelopmentPreview(1280, 720);
				PendingPreviewCaptureResolution = FIntPoint::ZeroValue;
			}
		}));
#endif
}

void USOTMIngameUIWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureProductionHUD();
	HideForestHealthPresentation();

	BoundPlayerState = USOTMPlayerBlueprintLibrary::GetPlayerStateSubsystem(this);
	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		BoundPlayerState->OnCoinsChanged.AddDynamic(this, &ThisClass::HandleCoinsChanged);
		BoundPlayerState->OnLivesChanged.RemoveDynamic(this, &ThisClass::HandleLivesChanged);
		BoundPlayerState->OnLivesChanged.AddDynamic(this, &ThisClass::HandleLivesChanged);
		BoundPlayerState->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		BoundPlayerState->OnPlayerDeathStarted.AddDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		BoundPlayerState->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
		BoundPlayerState->OnPlayerRespawned.AddDynamic(this, &ThisClass::HandlePlayerRespawned);
		BoundPlayerState->OnGameOver.RemoveDynamic(this, &ThisClass::HandleGameOver);
		BoundPlayerState->OnGameOver.AddDynamic(this, &ThisClass::HandleGameOver);
		BoundPlayerState->OnInputLocksChanged.RemoveDynamic(this, &ThisClass::HandleInputLocksChanged);
		BoundPlayerState->OnInputLocksChanged.AddDynamic(this, &ThisClass::HandleInputLocksChanged);
		BoundPlayerState->OnPhase4ProgressChanged.RemoveDynamic(this, &ThisClass::HandlePhase4ProgressChanged);
		BoundPlayerState->OnPhase4ProgressChanged.AddDynamic(this, &ThisClass::HandlePhase4ProgressChanged);

		RefreshCoinCounter(
			BoundPlayerState->GetAvailableCoins(),
			BoundPlayerState->GetLifetimeCoinsCollected(),
			false);
		RefreshLives(BoundPlayerState->GetCurrentLives(), BoundPlayerState->GetMaximumLives(), false);
		HandleInputLocksChanged(
			BoundPlayerState->HasAnyInputLock(),
			BoundPlayerState->GetActiveInputLockReasons());
		HandlePhase4ProgressChanged(
			BoundPlayerState->IsPhase4ChestOpened(), BoundPlayerState->HasPhase4GateKey(),
			BoundPlayerState->IsPhase4GateUnlocked(), BoundPlayerState->IsPhase4DemoCompleted());
	}
	else
	{
		RefreshCoinCounter(0, 0, false);
		RefreshLives(0, 0, false);
	}

	BoundObjectiveState = GetGameInstance()
		? GetGameInstance()->GetSubsystem<USOTMObjectiveSubsystem>() : nullptr;
	if (BoundObjectiveState)
	{
		BoundObjectiveState->OnObjectiveChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveChanged);
		BoundObjectiveState->OnObjectiveChanged.AddDynamic(this, &ThisClass::HandleObjectiveChanged);
		RefreshObjectivePresentation(BoundObjectiveState->GetCollectAllForestCoinsObjective());
	}
	else
	{
		RefreshObjectivePresentation(FSOTMObjectiveData());
	}

	BoundPhase2World = GetWorld() ? GetWorld()->GetSubsystem<USOTMDemoPhase2WorldSubsystem>() : nullptr;
	if (BoundPhase2World)
	{
		BoundPhase2World->OnCousinWarningChanged.RemoveDynamic(this, &ThisClass::HandleCousinWarningChanged);
		BoundPhase2World->OnCousinWarningChanged.AddDynamic(this, &ThisClass::HandleCousinWarningChanged);
	}

	BoundPhase3World = GetWorld() ? GetWorld()->GetSubsystem<USOTMDemoPhase3WorldSubsystem>() : nullptr;
	if (BoundPhase3World)
	{
		BoundPhase3World->OnStationPromptChanged.RemoveDynamic(this, &ThisClass::HandleStationPromptChanged);
		BoundPhase3World->OnStationPromptChanged.AddDynamic(this, &ThisClass::HandleStationPromptChanged);
		BoundPhase3World->OnSpeedBoostStateChanged.RemoveDynamic(this, &ThisClass::HandleSpeedBoostStateChanged);
		BoundPhase3World->OnSpeedBoostStateChanged.AddDynamic(this, &ThisClass::HandleSpeedBoostStateChanged);
		HandleSpeedBoostStateChanged(BoundPhase3World->GetSpeedBoostState(), 0.0f, 0.0f);
	}

	BoundLightningWorld = GetWorld()
		? GetWorld()->GetSubsystem<USOTMLightningThrowWorldSubsystem>() : nullptr;
	if (BoundLightningWorld)
	{
		BoundLightningWorld->OnLightningThrowStateChanged.RemoveDynamic(
			this, &ThisClass::HandleLightningThrowStateChanged);
		BoundLightningWorld->OnLightningThrowStateChanged.AddDynamic(
			this, &ThisClass::HandleLightningThrowStateChanged);
		HandleLightningThrowStateChanged(BoundLightningWorld->GetRuntimeState(), 0.0f, 0.0f);
	}

	BoundPhase4World = GetWorld() ? GetWorld()->GetSubsystem<USOTMDemoPhase4WorldSubsystem>() : nullptr;
	if (BoundPhase4World)
	{
		BoundPhase4World->OnPromptChanged.RemoveDynamic(this, &ThisClass::HandlePhase4PromptChanged);
		BoundPhase4World->OnPromptChanged.AddDynamic(this, &ThisClass::HandlePhase4PromptChanged);
		BoundPhase4World->OnNotification.RemoveDynamic(this, &ThisClass::HandlePhase4Notification);
		BoundPhase4World->OnNotification.AddDynamic(this, &ThisClass::HandlePhase4Notification);
	}

	BoundGateActor = GetWorld()
		? Cast<ASOTMKeyGateActor>(UGameplayStatics::GetActorOfClass(GetWorld(), ASOTMKeyGateActor::StaticClass()))
		: nullptr;
	if (BoundGateActor)
	{
		BoundGateActor->OnGatePromptChanged.RemoveDynamic(this, &ThisClass::HandleGatePromptChanged);
		BoundGateActor->OnGatePromptChanged.AddDynamic(this, &ThisClass::HandleGatePromptChanged);
	}

#if !UE_BUILD_SHIPPING
	ActiveDevelopmentHUD = this;
	if (bPendingObjectivePreview)
	{
		ShowDevelopmentObjectivePreview();
		bPendingObjectivePreview = false;
	}
	if (bPendingUpgradePreview)
	{
		ShowDevelopmentUpgradePreview();
		bPendingUpgradePreview = false;
	}
	if (bPendingBossPreview)
	{
		ShowDevelopmentBossPreview();
		bPendingBossPreview = false;
	}
	if (PendingPreviewCaptureResolution != FIntPoint::ZeroValue)
	{
		const FIntPoint Resolution = PendingPreviewCaptureResolution;
		PendingPreviewCaptureResolution = FIntPoint::ZeroValue;
		CaptureDevelopmentPreview(Resolution.X, Resolution.Y);
	}
#endif
}

void USOTMIngameUIWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CoinPulseTimerHandle);
		World->GetTimerManager().ClearTimer(LivesPulseTimerHandle);
		World->GetTimerManager().ClearTimer(Phase4NotificationTimerHandle);
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		BoundPlayerState->OnLivesChanged.RemoveDynamic(this, &ThisClass::HandleLivesChanged);
		BoundPlayerState->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		BoundPlayerState->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
		BoundPlayerState->OnGameOver.RemoveDynamic(this, &ThisClass::HandleGameOver);
		BoundPlayerState->OnInputLocksChanged.RemoveDynamic(this, &ThisClass::HandleInputLocksChanged);
		BoundPlayerState->OnPhase4ProgressChanged.RemoveDynamic(this, &ThisClass::HandlePhase4ProgressChanged);
	}
	BoundPlayerState = nullptr;
	if (BoundObjectiveState)
	{
		BoundObjectiveState->OnObjectiveChanged.RemoveDynamic(this, &ThisClass::HandleObjectiveChanged);
	}
	if (BoundPhase2World)
	{
		BoundPhase2World->OnCousinWarningChanged.RemoveDynamic(this, &ThisClass::HandleCousinWarningChanged);
	}
	if (BoundPhase3World)
	{
		BoundPhase3World->OnStationPromptChanged.RemoveDynamic(this, &ThisClass::HandleStationPromptChanged);
		BoundPhase3World->OnSpeedBoostStateChanged.RemoveDynamic(this, &ThisClass::HandleSpeedBoostStateChanged);
	}
	if (BoundLightningWorld)
	{
		BoundLightningWorld->OnLightningThrowStateChanged.RemoveDynamic(
			this, &ThisClass::HandleLightningThrowStateChanged);
	}
	if (BoundPhase4World)
	{
		BoundPhase4World->OnPromptChanged.RemoveDynamic(this, &ThisClass::HandlePhase4PromptChanged);
		BoundPhase4World->OnNotification.RemoveDynamic(this, &ThisClass::HandlePhase4Notification);
	}
	if (BoundGateActor)
	{
		BoundGateActor->OnGatePromptChanged.RemoveDynamic(this, &ThisClass::HandleGatePromptChanged);
	}
	BoundObjectiveState = nullptr;
	BoundPhase2World = nullptr;
	BoundPhase3World = nullptr;
	BoundLightningWorld = nullptr;
	BoundPhase4World = nullptr;
	BoundGateActor = nullptr;

#if !UE_BUILD_SHIPPING
	if (ActiveDevelopmentHUD.Get() == this)
	{
		ActiveDevelopmentHUD.Reset();
	}
#endif

	Super::NativeDestruct();
}

void USOTMIngameUIWidget::EnsureProductionHUD()
{
	// HUD visuals are now built entirely inside WBP_InGameMain's Designer
	// canvas and wired up automatically via the BindWidgetOptional properties
	// declared in the header - this function intentionally does not construct
	// any widgets anymore. It used to hand-build every panel in C++, which
	// duplicated the WBP-built panels once their names matched and produced
	// the HUD showing twice. If ObjectivePanel is still null here, the WBP
	// binding did not take (wrong widget class spawned, or a widget name
	// mismatch in the Designer) and the HUD will simply be blank rather than
	// silently falling back to a second, code-built copy.
	if (!ObjectivePanel)
	{
		UE_LOG(LogSOTMHUD, Warning,
			TEXT("ObjectivePanel is null - WBP_InGameMain did not bind. Check the spawned widget class and widget names in the Designer."));
	}
}

void USOTMIngameUIWidget::HandleCoinsChanged(const int32 AvailableCoins, const int32 LifetimeCoinsCollected)
{
	RefreshCoinCounter(AvailableCoins, LifetimeCoinsCollected, true);
}

void USOTMIngameUIWidget::HandleLivesChanged(const int32 CurrentLives, const int32 MaximumLives)
{
	RefreshLives(CurrentLives, MaximumLives, true);
}

void USOTMIngameUIWidget::RefreshCoinCounter(
	const int32 AvailableCoins,
	const int32 LifetimeCoinsCollected,
	const bool bPlayFeedback)
{
	if (!CoinCounterText)
	{
		return;
	}

	const int32 SafeAvailable = FMath::Max(0, AvailableCoins);
	const int32 SafeLifetime = FMath::Max(0, LifetimeCoinsCollected);
	const bool bIncreased = DisplayedAvailableCoins != INDEX_NONE && SafeAvailable > DisplayedAvailableCoins;
	DisplayedAvailableCoins = SafeAvailable;
	CoinCounterText->SetText(FText::Format(
		NSLOCTEXT("SOTM", "CoinCounterFormat", "COINS COLLECTED   {0}"),
		FText::AsNumber(SafeLifetime)));
	if (TopRightCoinText)
	{
		// The client's objective panel spec asks for the coins still needed for the next
		// upgrade, so the counter names the cheapest ability the player does not own yet.
		const USOTMPlayerStateSubsystem* State = BoundPlayerState;
		int32 NextUpgradeCost = 0;
		if (State && !State->IsSpeedBoostUnlocked())
		{
			NextUpgradeCost = GetDefault<USOTMPhase3Settings>()->SpeedBoostUnlockCost;
		}
		else if (State && !State->IsLightningThrowUnlocked())
		{
			NextUpgradeCost = GetDefault<USOTMLightningThrowSettings>()->LightningThrowUnlockCost;
		}
		TopRightCoinText->SetText(NextUpgradeCost > 0
			? FText::Format(
				NSLOCTEXT("SOTM", "TopRightCoinUpgradeFormat", "COINS   {0}   /   NEXT UPGRADE   {1}"),
				FText::AsNumber(SafeAvailable), FText::AsNumber(NextUpgradeCost))
			: FText::Format(
				NSLOCTEXT("SOTM", "TopRightCoinCounterFormat", "COINS   {0}"),
				FText::AsNumber(SafeAvailable)));
	}

	if (bPlayFeedback && bIncreased)
	{
		FWidgetTransform Pulse;
		Pulse.Scale = FVector2D(1.10f, 1.10f);
		CoinCounterText->SetRenderTransform(Pulse);
		CoinCounterText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.96f, 0.50f, 1.0f)));
		if (TopRightCoinText)
		{
			TopRightCoinText->SetRenderTransform(Pulse);
			TopRightCoinText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.96f, 0.50f, 1.0f)));
		}
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CoinPulseTimerHandle);
			World->GetTimerManager().SetTimer(
				CoinPulseTimerHandle, this, &ThisClass::FinishCoinPulse, 0.16f, false);
		}
	}
}

void USOTMIngameUIWidget::RefreshLives(
	const int32 CurrentLives,
	const int32 MaximumLives,
	const bool bPlayFeedback)
{
	if (!LivesText)
	{
		return;
	}

	const int32 SafeLives = FMath::Max(0, CurrentLives);
	const int32 SafeMaximum = FMath::Max(0, MaximumLives);
	const bool bDecreased = DisplayedLives != INDEX_NONE && SafeLives < DisplayedLives;
	DisplayedLives = SafeLives;
	LivesText->SetText(FText::Format(
		NSLOCTEXT("SOTM", "LivesFormat", "LIVES   {0} / {1}"),
		FText::AsNumber(SafeLives), FText::AsNumber(SafeMaximum)));

	if (bPlayFeedback && bDecreased)
	{
		FWidgetTransform Pulse;
		Pulse.Scale = FVector2D(1.10f, 1.10f);
		LivesText->SetRenderTransform(Pulse);
		LivesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.22f, 0.22f, 1.0f)));
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(LivesPulseTimerHandle);
			World->GetTimerManager().SetTimer(
				LivesPulseTimerHandle, this, &ThisClass::FinishLivesPulse, 0.24f, false);
		}
	}
}

void USOTMIngameUIWidget::FinishCoinPulse()
{
	if (CoinCounterText)
	{
		CoinCounterText->SetRenderTransform(FWidgetTransform());
		CoinCounterText->SetColorAndOpacity(FSlateColor(ObjectiveTitleColor));
	}
	if (TopRightCoinText)
	{
		TopRightCoinText->SetRenderTransform(FWidgetTransform());
		TopRightCoinText->SetColorAndOpacity(FSlateColor(GoldAccent));
	}
}

void USOTMIngameUIWidget::FinishLivesPulse()
{
	if (LivesText)
	{
		LivesText->SetRenderTransform(FWidgetTransform());
		LivesText->SetColorAndOpacity(FSlateColor(PurpleAccent));
	}
}

void USOTMIngameUIWidget::HandleObjectiveChanged(const FSOTMObjectiveData Objective)
{
	RefreshObjectivePresentation(Objective);
}

void USOTMIngameUIWidget::RefreshObjectivePresentation(const FSOTMObjectiveData& Objective)
{
	(void)Objective;
	const bool bForestObjectiveActive = BoundObjectiveState && BoundObjectiveState->IsForestObjectiveActive();
	const ESlateVisibility ForestHUDVisibility = bForestObjectiveActive
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed;

	if (ObjectivePanel)
	{
		ObjectivePanel->SetVisibility(ForestHUDVisibility);
	}
	if (TopRightCoinPanel)
	{
		TopRightCoinPanel->SetVisibility(ForestHUDVisibility);
	}
	if (SpeedBoostPanel)
	{
		SpeedBoostPanel->SetVisibility(ForestHUDVisibility);
	}
	if (GateKeyPanel)
	{
		GateKeyPanel->SetVisibility(ForestHUDVisibility);
	}
	if (CurrentObjectiveSection)
	{
		CurrentObjectiveSection->SetVisibility(ForestHUDVisibility);
	}

	if (!bForestObjectiveActive || !CurrentObjectiveText || !ObjectiveProgressText)
	{
		return;
	}

	const TArray<FSOTMObjectiveData> Objectives = BoundObjectiveState->GetChapterOneObjectives();
	const FSOTMObjectiveData Active = BoundObjectiveState->GetActiveChapterOneObjective();
	if (!Active.ObjectiveId.IsNone())
	{
		const bool bCompleted = Active.State == ESOTMObjectiveState::Completed;
		CurrentObjectiveText->SetText(FText::Format(
			bCompleted
				? NSLOCTEXT("SOTM", "ObjectiveCompleteFormat", "[COMPLETE]  {0}")
				: NSLOCTEXT("SOTM", "ObjectiveActiveFormat", "[ACTIVE]  {0}"),
			Active.DisplayName));
		CurrentObjectiveText->SetColorAndOpacity(FSlateColor(bCompleted ? ObjectiveTitleColor : ObjectiveBodyColor));
		ObjectiveProgressText->SetText(Active.ObjectiveId == USOTMObjectiveSubsystem::CollectAllForestCoinsId
			? FText::Format(NSLOCTEXT("SOTM", "ForestCoinsProgress", "{0} / {1}"),
				FText::AsNumber(FMath::Max(0, Active.CurrentProgress)),
				FText::AsNumber(USOTMObjectiveSubsystem::TotalForestCoins))
			: (bCompleted ? NSLOCTEXT("SOTM", "ObjectiveCompleteShort", "COMPLETE")
				: NSLOCTEXT("SOTM", "ObjectiveInProgress", "IN PROGRESS")));
	}

	if (FutureObjectivesText)
	{
		FString Rows;
		for (const FSOTMObjectiveData& Item : Objectives)
		{
			if (Item.ObjectiveId == Active.ObjectiveId ||
				Item.ObjectiveId == USOTMObjectiveSubsystem::CollectAllForestCoinsId ||
				Item.ObjectiveId == USOTMObjectiveSubsystem::DemoCompleteId)
			{
				continue;
			}
			const TCHAR* Prefix = Item.State == ESOTMObjectiveState::Completed ? TEXT("[DONE]")
				: (Item.State == ESOTMObjectiveState::Active ? TEXT("[ACTIVE]") : TEXT("\u25C6 [LOCKED]"));
			Rows += FString::Printf(TEXT("%s  %s\n\n"), Prefix, *Item.DisplayName.ToString());
		}
		Rows.RemoveFromEnd(TEXT("\n\n"));
		FutureObjectivesText->SetText(FText::FromString(Rows));
	}
}

void USOTMIngameUIWidget::HandlePhase4ProgressChanged(
	const bool bChestOpened,
	const bool bHasGateKey,
	const bool bGateUnlocked,
	const bool bDemoCompleted)
{
	(void)bChestOpened;
	(void)bGateUnlocked;
	(void)bDemoCompleted;
	if (GateKeyText)
	{
		GateKeyText->SetText(bHasGateKey
			? NSLOCTEXT("SOTM", "GateKeyAcquiredHUD", "GATE KEY\nACQUIRED")
			: NSLOCTEXT("SOTM", "GateKeyNotAcquired", "GATE KEY\nNOT ACQUIRED"));
	}
	if (BoundObjectiveState)
	{
		RefreshObjectivePresentation(BoundObjectiveState->GetActiveChapterOneObjective());
	}
}

void USOTMIngameUIWidget::HandlePhase4PromptChanged(const bool bVisible, const FText PromptText)
{
	if (!Phase4PromptPanel || !Phase4PromptText)
	{
		return;
	}
	Phase4PromptText->SetText(PromptText);
	Phase4PromptPanel->SetVisibility(bVisible && !PromptText.IsEmptyOrWhitespace()
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void USOTMIngameUIWidget::HandleGatePromptChanged(const bool bVisible, const FText PromptText)
{
	// Reuses the same Phase4PromptPanel/Phase4PromptText widgets the Phase 4 demo
	// interactables already use - no new widgets needed in WBP_InGameMain. The two
	// prompt sources are never expected to be active at the same time in practice.
	if (!Phase4PromptPanel || !Phase4PromptText)
	{
		return;
	}
	Phase4PromptText->SetText(PromptText);
	Phase4PromptPanel->SetVisibility(bVisible && !PromptText.IsEmptyOrWhitespace()
		? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void USOTMIngameUIWidget::HandlePhase4Notification(const FText Title, const FText Detail)
{
	if (!Phase4NotificationPanel || !Phase4NotificationTitle || !Phase4NotificationDetail)
	{
		return;
	}
	Phase4NotificationTitle->SetText(Title);
	Phase4NotificationDetail->SetText(Detail);
	Phase4NotificationPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(Phase4NotificationTimerHandle);
		World->GetTimerManager().SetTimer(
			Phase4NotificationTimerHandle, this, &ThisClass::HidePhase4Notification, 4.0f, false);
	}
}

void USOTMIngameUIWidget::HidePhase4Notification()
{
	if (Phase4NotificationPanel)
	{
		Phase4NotificationPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMIngameUIWidget::HideForestHealthPresentation()
{
	const UWorld* World = GetWorld();
	if (!World || FName(*UWorld::RemovePIEPrefix(World->GetOutermost()->GetName())) !=
		FName(TEXT("/Game/MenuSystemPro/ExampleContent/Designs/Design_Silence/Levels/CH1")))
	{
		return;
	}
	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (Widget && Widget->GetName().Contains(TEXT("Health"), ESearchCase::IgnoreCase))
		{
			Widget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void USOTMIngameUIWidget::HandleCousinWarningChanged(const bool bVisible)
{
	if (!CousinWarningPanel)
	{
		return;
	}

	const bool bForestObjectiveActive = BoundObjectiveState && BoundObjectiveState->IsForestObjectiveActive();
	CousinWarningPanel->SetVisibility(bVisible && bForestObjectiveActive
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void USOTMIngameUIWidget::HandleStationPromptChanged(const bool bVisible)
{
	if (!StationPromptPanel)
	{
		return;
	}
	// Timmy's Upgrade Station also exists outside the Forest map (e.g. the CH1
	// Mansion), so gating this on IsForestObjectiveActive() (true only in the
	// Forest map) was silently hiding the prompt anywhere else the station is
	// placed even while the player was genuinely standing in range. Show/hide it
	// purely off the station's own overlap state instead.
	StationPromptPanel->SetVisibility(bVisible
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void USOTMIngameUIWidget::HandleLightningThrowStateChanged(
	const ESOTMLightningThrowRuntimeState State,
	const float RemainingSeconds,
	const float NormalizedRemaining)
{
	if (!LightningThrowText)
	{
		return;
	}
	FNumberFormattingOptions CountdownFormat;
	CountdownFormat.SetMaximumFractionalDigits(1);
	CountdownFormat.SetMinimumFractionalDigits(1);

	FText StateText;
	FLinearColor StateColor = PurpleAccent;
	bool bShowProgress = false;
	switch (State)
	{
	case ESOTMLightningThrowRuntimeState::Ready:
		StateText = NSLOCTEXT("SOTM", "LightningReadyHUD", "LIGHTNING THROW [F]\nREADY");
		StateColor = FLinearColor(0.35f, 0.90f, 0.22f, 1.0f);
		break;
	case ESOTMLightningThrowRuntimeState::Cooldown:
		// Floating-point countdown (e.g. 4.0 -> 3.9 -> ... -> 0.1), driven by the world
		// subsystem's 0.1s cooldown ticker, starting from LightningThrowCooldown (4.0s).
		StateText = FText::Format(
			NSLOCTEXT("SOTM", "LightningCooldownHUD", "LIGHTNING THROW\nCOOLDOWN  {0}s"),
			FText::AsNumber(FMath::Max(0.0f, RemainingSeconds), &CountdownFormat));
		StateColor = PurpleAccent;
		bShowProgress = true;
		break;
	case ESOTMLightningThrowRuntimeState::Locked:
	default:
		StateText = NSLOCTEXT("SOTM", "LightningLockedHUDState", "LIGHTNING THROW\nLOCKED");
		StateColor = RedAccent;
		break;
	}
	LightningThrowText->SetText(StateText);
	LightningThrowText->SetColorAndOpacity(FSlateColor(StateColor));
	if (LightningThrowProgressBar)
	{
		LightningThrowProgressBar->SetPercent(FMath::Clamp(NormalizedRemaining, 0.0f, 1.0f));
		LightningThrowProgressBar->SetVisibility(bShowProgress
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void USOTMIngameUIWidget::HandleSpeedBoostStateChanged(
	const ESOTMSpeedBoostRuntimeState State,
	const float RemainingSeconds,
	const float NormalizedRemaining)
{
	if (!SpeedBoostText || !SpeedBoostPanel || !SpeedBoostProgressBar)
	{
		return;
	}

	FText StateText;
	FLinearColor StateColor = PurpleAccent;
	bool bShowProgress = false;
	FNumberFormattingOptions CountdownFormat;
	CountdownFormat.SetMaximumFractionalDigits(1);
	CountdownFormat.SetMinimumFractionalDigits(1);
	switch (State)
	{
	case ESOTMSpeedBoostRuntimeState::Ready:
		StateText = NSLOCTEXT("SOTM", "SpeedBoostReady", "SPEED BOOST [Q]\nREADY");
		StateColor = FLinearColor(0.35f, 0.90f, 0.22f, 1.0f);
		break;
	case ESOTMSpeedBoostRuntimeState::Active:
		StateText = FText::Format(
			NSLOCTEXT("SOTM", "SpeedBoostActive", "SPEED BOOST\nACTIVE  {0}s"),
			FText::AsNumber(FMath::Max(0.0f, RemainingSeconds), &CountdownFormat));
		StateColor = FLinearColor(0.20f, 0.62f, 1.0f, 1.0f);
		bShowProgress = true;
		break;
	case ESOTMSpeedBoostRuntimeState::Cooldown:
		StateText = FText::Format(
			NSLOCTEXT("SOTM", "SpeedBoostCooldown", "SPEED BOOST\nCOOLDOWN  {0}s"),
			FText::AsNumber(FMath::Max(0.0f, RemainingSeconds), &CountdownFormat));
		StateColor = PurpleAccent;
		bShowProgress = true;
		break;
	case ESOTMSpeedBoostRuntimeState::Locked:
	default:
		StateText = NSLOCTEXT("SOTM", "SpeedBoostLockedPhase3", "SPEED BOOST\nLOCKED");
		StateColor = RedAccent;
		break;
	}

	SpeedBoostText->SetText(StateText);
	SpeedBoostText->SetColorAndOpacity(FSlateColor(StateColor));
	// The panel border now has an image texture background (Designer change), so
	// tinting it near-black here was crushing that texture to black. Keep the brush
	// at full white/opaque so the texture shows its own natural colors.
	SpeedBoostPanel->SetBrushColor(FLinearColor::White);
	SpeedBoostProgressBar->SetPercent(FMath::Clamp(NormalizedRemaining, 0.0f, 1.0f));
	SpeedBoostProgressBar->SetVisibility(bShowProgress
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

void USOTMIngameUIWidget::SetCurrentObjective(const FText& ObjectiveText)
{
	if (!CurrentObjectiveSection || !CurrentObjectiveText)
	{
		return;
	}
	CurrentObjectiveText->SetText(ObjectiveText);
	CurrentObjectiveSection->SetVisibility(
		ObjectiveText.IsEmptyOrWhitespace() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void USOTMIngameUIWidget::ClearCurrentObjective()
{
	SetCurrentObjective(FText::GetEmpty());
}

void USOTMIngameUIWidget::SetRequiredCoins(const int32 CurrentCoins, const int32 RequiredCoins)
{
	if (!RequiredCoinsSection || !RequiredCoinsText)
	{
		return;
	}
	if (RequiredCoins <= 0)
	{
		ClearRequiredCoins();
		return;
	}
	RequiredCoinsText->SetText(FText::Format(
		NSLOCTEXT("SOTM", "RequiredCoinsFormat", "{0} / {1}"),
		FText::AsNumber(FMath::Max(0, CurrentCoins)),
		FText::AsNumber(RequiredCoins)));
	RequiredCoinsSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USOTMIngameUIWidget::ClearRequiredCoins()
{
	if (RequiredCoinsSection)
	{
		RequiredCoinsSection->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMIngameUIWidget::SetUpgradeProgress(const float Progress, const FText& DetailText)
{
	if (!UpgradeSection || !UpgradeProgressBar || !UpgradeDetailText)
	{
		return;
	}
	UpgradeProgressBar->SetPercent(FMath::Clamp(Progress, 0.0f, 1.0f));
	UpgradeDetailText->SetText(DetailText);
	UpgradeSection->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USOTMIngameUIWidget::ClearUpgradeProgress()
{
	if (UpgradeSection)
	{
		UpgradeSection->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMIngameUIWidget::SetBossProgress(const FText& BossName, const float NormalizedHealth)
{
	if (!BossPanel || !BossNameText || !BossProgressBar)
	{
		return;
	}
	BossNameText->SetText(FText::Format(
		NSLOCTEXT("SOTM", "BossNameFormat", "BOSS   {0}"), BossName));
	BossProgressBar->SetPercent(FMath::Clamp(NormalizedHealth, 0.0f, 1.0f));
	BossPanel->SetVisibility(
		BossName.IsEmptyOrWhitespace() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void USOTMIngameUIWidget::ClearBossProgress()
{
	if (BossPanel)
	{
		BossPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMIngameUIWidget::SetMissionTask(
	const FName TaskId,
	const FText& TaskText,
	const bool bCompleted)
{
	if (TaskId.IsNone() || TaskText.IsEmptyOrWhitespace() || !MissionTasksContainer || !WidgetTree)
	{
		return;
	}

	UTextBlock* Row = MissionTaskRows.FindRef(TaskId);
	if (!Row)
	{
		Row = CreateText(WidgetTree, NAME_None, FText::GetEmpty(), 15, ObjectiveBodyColor);
		Row->SetAutoWrapText(true);
		Row->SetWrapTextAt(335.0f);
		AddVertical(MissionTasksContainer, Row, FMargin(0.0f, 1.0f));
		MissionTaskRows.Add(TaskId, Row);
	}

	Row->SetText(FText::Format(
		bCompleted
			? NSLOCTEXT("SOTM", "CompletedTaskFormat", "[DONE]  {0}")
			: NSLOCTEXT("SOTM", "ActiveTaskFormat", "-  {0}"),
		TaskText));
	Row->SetColorAndOpacity(FSlateColor(bCompleted ? ObjectiveMutedColor : ObjectiveBodyColor));
	MissionTasksHeader->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	MissionTasksContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void USOTMIngameUIWidget::RemoveMissionTask(const FName TaskId)
{
	if (TObjectPtr<UTextBlock>* Row = MissionTaskRows.Find(TaskId))
	{
		if (MissionTasksContainer && Row->Get())
		{
			MissionTasksContainer->RemoveChild(Row->Get());
		}
		MissionTaskRows.Remove(TaskId);
	}
	if (MissionTaskRows.IsEmpty())
	{
		MissionTasksHeader->SetVisibility(ESlateVisibility::Collapsed);
		MissionTasksContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void USOTMIngameUIWidget::ClearMissionTasks()
{
	if (MissionTasksContainer)
	{
		MissionTasksContainer->ClearChildren();
		MissionTasksContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MissionTasksHeader)
	{
		MissionTasksHeader->SetVisibility(ESlateVisibility::Collapsed);
	}
	MissionTaskRows.Reset();
}

void USOTMIngameUIWidget::SetHUDPresentationVisible(const bool bVisible)
{
	bHUDPresentationRequested = bVisible;
	RefreshPresentationVisibility();
}

void USOTMIngameUIWidget::HandlePlayerDeathStarted(AActor* PlayerActor)
{
	(void)PlayerActor;
	bSuppressedByGameplayState = true;
	RefreshPresentationVisibility();
}

void USOTMIngameUIWidget::HandlePlayerRespawned(AActor* PlayerActor)
{
	(void)PlayerActor;
	if (BoundPlayerState)
	{
		RefreshCoinCounter(
			BoundPlayerState->GetAvailableCoins(),
			BoundPlayerState->GetLifetimeCoinsCollected(), false);
		RefreshLives(BoundPlayerState->GetCurrentLives(), BoundPlayerState->GetMaximumLives(), false);
		HandleInputLocksChanged(
			BoundPlayerState->HasAnyInputLock(), BoundPlayerState->GetActiveInputLockReasons());
	}
}

void USOTMIngameUIWidget::HandleGameOver()
{
	bSuppressedByGameplayState = true;
	RefreshPresentationVisibility();
}

void USOTMIngameUIWidget::HandleInputLocksChanged(
	const bool bInputLocked,
	const TArray<ESOTMInputLockReason> ActiveReasons)
{
	(void)bInputLocked;
	bSuppressedByGameplayState = ActiveReasons.Contains(ESOTMInputLockReason::Death) ||
		ActiveReasons.Contains(ESOTMInputLockReason::Respawn) ||
		ActiveReasons.Contains(ESOTMInputLockReason::Cinematic) ||
		ActiveReasons.Contains(ESOTMInputLockReason::JumpScare) ||
		ActiveReasons.Contains(ESOTMInputLockReason::GameOver);
	RefreshPresentationVisibility();
}

void USOTMIngameUIWidget::RefreshPresentationVisibility()
{
	SetVisibility(bHUDPresentationRequested && !bSuppressedByGameplayState
		? ESlateVisibility::SelfHitTestInvisible
		: ESlateVisibility::Collapsed);
}

#if !UE_BUILD_SHIPPING
void USOTMIngameUIWidget::ShowDevelopmentObjectivePreview()
{
	SetCurrentObjective(NSLOCTEXT(
		"SOTM", "HUDPreviewObjective", "Find a path through the forest and recover your stolen power"));
	SetMissionTask(TEXT("PreviewKey"), NSLOCTEXT("SOTM", "HUDPreviewTaskKey", "Find the hidden key"), false);
	SetMissionTask(TEXT("PreviewGate"), NSLOCTEXT("SOTM", "HUDPreviewTaskGate", "Reach the ancient gate"), true);
	UE_LOG(LogSOTMHUD, Display, TEXT("Development-only Objective Panel preview shown; gameplay state unchanged."));
}

void USOTMIngameUIWidget::ShowDevelopmentUpgradePreview()
{
	const int32 Available = BoundPlayerState ? BoundPlayerState->GetAvailableCoins() : 20;
	SetRequiredCoins(Available, 50);
	SetUpgradeProgress(2.0f / 3.0f, NSLOCTEXT("SOTM", "HUDPreviewUpgrade", "2 / 3 fragments recovered"));
	UE_LOG(LogSOTMHUD, Display, TEXT("Development-only Upgrade HUD preview shown; gameplay state unchanged."));
}

void USOTMIngameUIWidget::ShowDevelopmentBossPreview()
{
	SetBossProgress(NSLOCTEXT("SOTM", "HUDPreviewBoss", "ISABEL"), 0.64f);
	UE_LOG(LogSOTMHUD, Display, TEXT("Development-only Boss HUD preview shown; gameplay state unchanged."));
}

void USOTMIngameUIWidget::ClearDevelopmentPreview()
{
	ClearCurrentObjective();
	ClearRequiredCoins();
	ClearUpgradeProgress();
	ClearMissionTasks();
	ClearBossProgress();
	UE_LOG(LogSOTMHUD, Display, TEXT("Development-only HUD preview cleared."));
}

void USOTMIngameUIWidget::CaptureDevelopmentPreview(const int32 Width, const int32 Height)
{
	ShowDevelopmentObjectivePreview();
	ShowDevelopmentUpgradePreview();
	ShowDevelopmentBossPreview();

	if (UWorld* World = GetWorld(); World && GEngine)
	{
		UE_LOG(LogSOTMHUD, Display,
			TEXT("Development HUD capture armed for requested viewport %dx%d."), Width, Height);
		FTimerHandle CaptureTimer;
		World->GetTimerManager().SetTimer(
			CaptureTimer,
			FTimerDelegate::CreateWeakLambda(this, [this]
			{
				if (UWorld* CaptureWorld = GetWorld())
				{
					FVector2D ViewportSize = FVector2D::ZeroVector;
					if (UGameViewportClient* Viewport = CaptureWorld->GetGameViewport())
					{
						Viewport->GetViewportSize(ViewportSize);
					}
					const FString CapturePath = FPaths::Combine(
						FPaths::ProjectSavedDir(), TEXT("Screenshots/WindowsEditor/SOTM_HUD_Preview.png"));
					FScreenshotRequest::RequestScreenshot(CapturePath, true, false);
					UE_LOG(LogSOTMHUD, Display,
						TEXT("Development HUD capture requested after widget construction; viewport=%.0fx%.0f path=%s"),
						ViewportSize.X, ViewportSize.Y, *CapturePath);
				}
			}),
			0.75f,
			false);
	}
}
#endif
