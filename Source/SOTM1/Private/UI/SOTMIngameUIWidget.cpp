#include "UI/SOTMIngameUIWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "SOTMPlayerBlueprintLibrary.h"
#include "TimerManager.h"
#include "UnrealClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMHUD, Log, All);

namespace
{
	const FLinearColor PanelColor(0.012f, 0.018f, 0.032f, 0.88f);
	const FLinearColor PrimaryTextColor(0.84f, 0.90f, 0.96f, 1.0f);
	const FLinearColor MutedTextColor(0.50f, 0.62f, 0.72f, 1.0f);
	const FLinearColor BlueAccent(0.08f, 0.52f, 0.92f, 1.0f);
	const FLinearColor GoldAccent(1.0f, 0.82f, 0.18f, 1.0f);

	UTextBlock* CreateText(
		UWidgetTree* Tree,
		const FName Name,
		const FText& Text,
		const int32 Size,
		const FLinearColor& Color)
	{
		UTextBlock* Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Widget->SetText(Text);
		Widget->SetColorAndOpacity(FSlateColor(Color));
		Widget->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Widget->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f));
		FSlateFontInfo Font = Widget->GetFont();
		Font.Size = Size;
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

		RefreshCoinCounter(
			BoundPlayerState->GetAvailableCoins(),
			BoundPlayerState->GetLifetimeCoinsCollected(),
			false);
		RefreshLives(BoundPlayerState->GetCurrentLives(), BoundPlayerState->GetMaximumLives(), false);
		HandleInputLocksChanged(
			BoundPlayerState->HasAnyInputLock(),
			BoundPlayerState->GetActiveInputLockReasons());
	}
	else
	{
		RefreshCoinCounter(0, 0, false);
		RefreshLives(0, 0, false);
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
	}

	if (BoundPlayerState)
	{
		BoundPlayerState->OnCoinsChanged.RemoveDynamic(this, &ThisClass::HandleCoinsChanged);
		BoundPlayerState->OnLivesChanged.RemoveDynamic(this, &ThisClass::HandleLivesChanged);
		BoundPlayerState->OnPlayerDeathStarted.RemoveDynamic(this, &ThisClass::HandlePlayerDeathStarted);
		BoundPlayerState->OnPlayerRespawned.RemoveDynamic(this, &ThisClass::HandlePlayerRespawned);
		BoundPlayerState->OnGameOver.RemoveDynamic(this, &ThisClass::HandleGameOver);
		BoundPlayerState->OnInputLocksChanged.RemoveDynamic(this, &ThisClass::HandleInputLocksChanged);
	}
	BoundPlayerState = nullptr;

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
	if (ObjectivePanel || !WidgetTree)
	{
		return;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		UE_LOG(LogSOTMHUD, Error, TEXT("Production WBP_IngameUI root is not a CanvasPanel."));
		return;
	}

	ObjectivePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SOTM_ObjectivePanel"));
	ObjectivePanel->SetBrushColor(PanelColor);
	ObjectivePanel->SetPadding(FMargin(18.0f, 14.0f));
	UVerticalBox* ObjectiveContent = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SOTM_ObjectiveContent"));
	ObjectivePanel->SetContent(ObjectiveContent);

	UTextBlock* PanelTitle = CreateText(
		WidgetTree, TEXT("SOTM_ObjectivePanelTitle"),
		NSLOCTEXT("SOTM", "ChapterStatus", "CHAPTER STATUS"), 14, BlueAccent);
	AddVertical(ObjectiveContent, PanelTitle, FMargin(0.0f, 0.0f, 0.0f, 7.0f));

	CurrentObjectiveSection = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SOTM_CurrentObjectiveSection"));
	UTextBlock* ObjectiveHeader = CreateText(
		WidgetTree, TEXT("SOTM_CurrentObjectiveHeader"),
		NSLOCTEXT("SOTM", "CurrentObjectiveHeader", "CURRENT OBJECTIVE"), 12, MutedTextColor);
	CurrentObjectiveText = CreateText(
		WidgetTree, TEXT("SOTM_CurrentObjectiveText"), FText::GetEmpty(), 20, PrimaryTextColor);
	CurrentObjectiveText->SetAutoWrapText(true);
	CurrentObjectiveText->SetWrapTextAt(340.0f);
	AddVertical(CurrentObjectiveSection, ObjectiveHeader, FMargin(0.0f));
	AddVertical(CurrentObjectiveSection, CurrentObjectiveText, FMargin(0.0f, 2.0f, 0.0f, 8.0f));
	CurrentObjectiveSection->SetVisibility(ESlateVisibility::Collapsed);
	AddVertical(ObjectiveContent, CurrentObjectiveSection, FMargin(0.0f));

	CoinCounterText = CreateText(
		WidgetTree, TEXT("SOTM_CoinCounterText"), FText::GetEmpty(), 19, GoldAccent);
	AddVertical(ObjectiveContent, CoinCounterText, FMargin(0.0f, 2.0f, 0.0f, 4.0f));

	RequiredCoinsSection = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SOTM_RequiredCoinsSection"));
	UTextBlock* RequiredHeader = CreateText(
		WidgetTree, TEXT("SOTM_RequiredCoinsHeader"),
		NSLOCTEXT("SOTM", "RequiredCoinsHeader", "COINS NEEDED"), 12, MutedTextColor);
	RequiredCoinsText = CreateText(
		WidgetTree, TEXT("SOTM_RequiredCoinsText"), FText::GetEmpty(), 18, PrimaryTextColor);
	AddVertical(RequiredCoinsSection, RequiredHeader, FMargin(0.0f));
	AddVertical(RequiredCoinsSection, RequiredCoinsText, FMargin(0.0f, 1.0f, 0.0f, 6.0f));
	RequiredCoinsSection->SetVisibility(ESlateVisibility::Collapsed);
	AddVertical(ObjectiveContent, RequiredCoinsSection, FMargin(0.0f));

	UpgradeSection = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SOTM_UpgradeSection"));
	UTextBlock* UpgradeHeader = CreateText(
		WidgetTree, TEXT("SOTM_UpgradeHeader"),
		NSLOCTEXT("SOTM", "UpgradeProgressHeader", "UPGRADE PROGRESS"), 12, MutedTextColor);
	USizeBox* UpgradeBarSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SOTM_UpgradeBarSize"));
	UpgradeBarSize->SetHeightOverride(9.0f);
	UpgradeProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("SOTM_UpgradeProgressBar"));
	UpgradeProgressBar->SetFillColorAndOpacity(BlueAccent);
	UpgradeBarSize->SetContent(UpgradeProgressBar);
	UpgradeDetailText = CreateText(
		WidgetTree, TEXT("SOTM_UpgradeDetailText"), FText::GetEmpty(), 14, PrimaryTextColor);
	AddVertical(UpgradeSection, UpgradeHeader, FMargin(0.0f));
	AddVertical(UpgradeSection, UpgradeBarSize, FMargin(0.0f, 3.0f, 0.0f, 2.0f));
	AddVertical(UpgradeSection, UpgradeDetailText, FMargin(0.0f, 1.0f, 0.0f, 6.0f));
	UpgradeSection->SetVisibility(ESlateVisibility::Collapsed);
	AddVertical(ObjectiveContent, UpgradeSection, FMargin(0.0f));

	MissionTasksHeader = CreateText(
		WidgetTree, TEXT("SOTM_MissionTasksHeader"),
		NSLOCTEXT("SOTM", "MissionTasksHeader", "MISSION TASKS"), 12, MutedTextColor);
	MissionTasksHeader->SetVisibility(ESlateVisibility::Collapsed);
	AddVertical(ObjectiveContent, MissionTasksHeader, FMargin(0.0f, 2.0f, 0.0f, 2.0f));
	MissionTasksContainer = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SOTM_MissionTasks"));
	MissionTasksContainer->SetVisibility(ESlateVisibility::Collapsed);
	AddVertical(ObjectiveContent, MissionTasksContainer, FMargin(0.0f));

	if (UCanvasPanelSlot* ObjectiveSlot = RootCanvas->AddChildToCanvas(ObjectivePanel))
	{
		ObjectiveSlot->SetAnchors(FAnchors(0.0f, 0.0f));
		ObjectiveSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		ObjectiveSlot->SetPosition(FVector2D(32.0f, 32.0f));
		ObjectiveSlot->SetAutoSize(true);
		ObjectiveSlot->SetZOrder(100);
	}

	LivesText = CreateText(
		WidgetTree, TEXT("SOTM_LivesText"), FText::GetEmpty(), 20, PrimaryTextColor);
	LivesText->SetJustification(ETextJustify::Left);
	if (UCanvasPanelSlot* LivesSlot = RootCanvas->AddChildToCanvas(LivesText))
	{
		LivesSlot->SetAnchors(FAnchors(0.0f, 1.0f));
		LivesSlot->SetAlignment(FVector2D(0.0f, 1.0f));
		LivesSlot->SetPosition(FVector2D(36.0f, -34.0f));
		LivesSlot->SetAutoSize(true);
		LivesSlot->SetZOrder(100);
	}

	BossPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SOTM_BossPanel"));
	BossPanel->SetBrushColor(PanelColor);
	BossPanel->SetPadding(FMargin(18.0f, 10.0f));
	UVerticalBox* BossContent = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SOTM_BossContent"));
	BossPanel->SetContent(BossContent);
	BossNameText = CreateText(
		WidgetTree, TEXT("SOTM_BossNameText"), FText::GetEmpty(), 18, PrimaryTextColor);
	BossNameText->SetJustification(ETextJustify::Center);
	USizeBox* BossBarSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SOTM_BossBarSize"));
	BossBarSize->SetWidthOverride(500.0f);
	BossBarSize->SetHeightOverride(12.0f);
	BossProgressBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("SOTM_BossProgressBar"));
	BossProgressBar->SetFillColorAndOpacity(FLinearColor(0.50f, 0.08f, 0.68f, 1.0f));
	BossBarSize->SetContent(BossProgressBar);
	AddVertical(BossContent, BossNameText, FMargin(0.0f, 0.0f, 0.0f, 5.0f));
	AddVertical(BossContent, BossBarSize, FMargin(0.0f));
	BossPanel->SetVisibility(ESlateVisibility::Collapsed);
	if (UCanvasPanelSlot* BossSlot = RootCanvas->AddChildToCanvas(BossPanel))
	{
		BossSlot->SetAnchors(FAnchors(0.5f, 0.0f));
		BossSlot->SetAlignment(FVector2D(0.5f, 0.0f));
		BossSlot->SetPosition(FVector2D(0.0f, 32.0f));
		BossSlot->SetAutoSize(true);
		BossSlot->SetZOrder(110);
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

	if (bPlayFeedback && bIncreased)
	{
		FWidgetTransform Pulse;
		Pulse.Scale = FVector2D(1.10f, 1.10f);
		CoinCounterText->SetRenderTransform(Pulse);
		CoinCounterText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.96f, 0.50f, 1.0f)));
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
		CoinCounterText->SetColorAndOpacity(FSlateColor(GoldAccent));
	}
}

void USOTMIngameUIWidget::FinishLivesPulse()
{
	if (LivesText)
	{
		LivesText->SetRenderTransform(FWidgetTransform());
		LivesText->SetColorAndOpacity(FSlateColor(PrimaryTextColor));
	}
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
		Row = CreateText(WidgetTree, NAME_None, FText::GetEmpty(), 15, PrimaryTextColor);
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
	Row->SetColorAndOpacity(FSlateColor(bCompleted ? MutedTextColor : PrimaryTextColor));
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
