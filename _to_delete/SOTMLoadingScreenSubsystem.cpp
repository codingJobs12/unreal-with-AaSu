#include "UI/SOTMLoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"

namespace SOTMLoadingScreenPrivate
{
	// DA_CH1's thumbnail: the "Chapter 1" artwork.
	const TCHAR* ChapterArtPath = TEXT("/Game/3b6dc938-2b23-4871-a137-61944389d60c.3b6dc938-2b23-4871-a137-61944389d60c");
	constexpr float TipInterval = 3.2f;

	const FText Tips[] =
	{
		NSLOCTEXT("SOTM", "LoadTip01", "Collect as many coins as you can to gain more superpowers."),
		NSLOCTEXT("SOTM", "LoadTip02", "Watch out for the Demon Girl and Spider Girl in their respective realms."),
		NSLOCTEXT("SOTM", "LoadTip03", "Use checkpoints wisely to save your progress."),
		NSLOCTEXT("SOTM", "LoadTip04", "Visit Timmy's upgrade station to spend your coins on new abilities."),
		NSLOCTEXT("SOTM", "LoadTip05", "Press Q to use Speed Boost - but use it wisely, you get tired after a while."),
		NSLOCTEXT("SOTM", "LoadTip06", "Lightning Throw is deadly, but when the Cousins get close, run."),
		NSLOCTEXT("SOTM", "LoadTip07", "Keep your distance from the Cousins. Once they spot you, don't look back."),
		NSLOCTEXT("SOTM", "LoadTip08", "Press E to interact with chests, stations and gates."),
		NSLOCTEXT("SOTM", "LoadTip09", "Somewhere in the forest, a chest hides the key to the gate."),
		NSLOCTEXT("SOTM", "LoadTip10", "Your lives are limited. Stay alert and never let the Cousins catch you."),
		NSLOCTEXT("SOTM", "LoadTip11", "Coins are hidden all over the forest - explore every corner."),
		NSLOCTEXT("SOTM", "LoadTip12", "Complete your objectives in order - the next one only unlocks after the last."),
	};
	constexpr int32 TipCount = UE_ARRAY_COUNT(Tips);
}

void USOTMLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	PollHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::PollEngineLoadingWidget), 0.1f);
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	PreLoadMapHandle = FCoreUObjectDelegates::PreLoadMap.AddUObject(this, &ThisClass::HandlePreLoadMap);
}

bool USOTMLoadingScreenSubsystem::PollEngineLoadingWidget(const float DeltaTime)
{
	(void)DeltaTime;
	if (Root.IsValid())
	{
		return true;
	}
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (!IsValid(Widget) || Widget->HasAnyFlags(RF_ClassDefaultObject) || !Widget->IsVisible())
		{
			continue;
		}
		const UWorld* WidgetWorld = Widget->GetWorld();
		if (!WidgetWorld || !WidgetWorld->IsGameWorld() || HandledEngineWidgets.Contains(Widget))
		{
			continue;
		}
		if (Widget->GetClass()->GetName().Contains(TEXT("LoadingScreen"), ESearchCase::IgnoreCase))
		{
			// Do NOT collapse it: a collapsed widget stops ticking, which stalls its own
			// loading logic (the level never opens). Keep it alive but invisible; the SOTM
			// screen is drawn above it anyway.
			Widget->SetRenderOpacity(0.0f);
			Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
			HandledEngineWidgets.Add(Widget);
			ShowLoadingScreen();
			UE_LOG(LogTemp, Display, TEXT("SOTM loading screen: replaced %s with the SOTM loading screen."), *Widget->GetClass()->GetName());
			break;
		}
	}
	return true;
}

void USOTMLoadingScreenSubsystem::HandlePreLoadMap(const FString& MapName)
{
	// Guarantees the SOTM loading screen covers the Mansion / CH1 hand-over even when the
	// Menu System Pro loading widget could not be detected: it is drawn as soon as the new
	// map is up and held until the Mansion intro / CH1 release it.
	if (!Root.IsValid() && (MapName.Contains(TEXT("Mansion_GameStart")) || MapName.Contains(TEXT("CH1"))))
	{
		UE_LOG(LogTemp, Display, TEXT("SOTM loading screen: showing for map %s (PreLoadMap)."), *MapName);
		ShowLoadingScreen();
	}
}

void USOTMLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!Root.IsValid() || !LoadedWorld)
	{
		return;
	}
	// The Mansion intro and CH1 release the screen themselves once they have settled; any
	// other map (e.g. back to the Main Menu) just clears it shortly after loading.
	const FString MapName = UWorld::RemovePIEPrefix(LoadedWorld->GetOutermost()->GetName());
	if (!MapName.Contains(TEXT("Mansion_GameStart")) && !MapName.Contains(TEXT("CH1")))
	{
		HideLoadingScreenAfter(1.0f);
	}
}

void USOTMLoadingScreenSubsystem::Deinitialize()
{
	if (PollHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
		PollHandle.Reset();
	}
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
	FCoreUObjectDelegates::PreLoadMap.Remove(PreLoadMapHandle);
	HideLoadingScreenNow();
	Super::Deinitialize();
}

void USOTMLoadingScreenSubsystem::RebuildTipOrder()
{
	using namespace SOTMLoadingScreenPrivate;
	TipOrder.Reset();
	for (int32 Index = 0; Index < TipCount; ++Index)
	{
		TipOrder.Add(Index);
	}
	// Fisher-Yates shuffle.
	for (int32 Index = TipOrder.Num() - 1; Index > 0; --Index)
	{
		TipOrder.Swap(Index, FMath::RandRange(0, Index));
	}
	// Never start a new round with the tip that just ended the previous one.
	if (TipOrder.Num() > 1 && TipOrder[0] == LastTipIndex)
	{
		TipOrder.Swap(0, TipOrder.Num() - 1);
	}
	TipCursor = 0;
}

void USOTMLoadingScreenSubsystem::ShowNextTip()
{
	using namespace SOTMLoadingScreenPrivate;
	if (!TipText.IsValid())
	{
		return;
	}
	if (TipCursor >= TipOrder.Num())
	{
		RebuildTipOrder();
	}
	LastTipIndex = TipOrder[TipCursor++];
	TipText->SetText(Tips[LastTipIndex]);
}

void USOTMLoadingScreenSubsystem::ShowLoadingScreen()
{
	using namespace SOTMLoadingScreenPrivate;
	if (Root.IsValid())
	{
		return;
	}
	UGameInstance* GameInstance = GetGameInstance();
	UGameViewportClient* GameViewport = GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
	if (!GameViewport)
	{
		return;
	}
	Viewport = GameViewport;

	BackgroundTexture = LoadObject<UTexture2D>(nullptr, ChapterArtPath);
	BackgroundBrush = MakeShared<FSlateBrush>();
	if (BackgroundTexture)
	{
		BackgroundBrush->SetResourceObject(BackgroundTexture);
		BackgroundBrush->ImageSize = FVector2D(BackgroundTexture->GetSizeX(), BackgroundTexture->GetSizeY());
		BackgroundBrush->DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SOTM loading screen: chapter art %s not found - using plain background."), ChapterArtPath);
		BackgroundBrush->DrawAs = ESlateBrushDrawType::NoDrawType;
	}

	const FSlateFontInfo TipFont = FCoreStyle::GetDefaultFontStyle("Regular", 16);

	Root = SNew(SOverlay)
		+ SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor::Black)
		]
		+ SOverlay::Slot()
		[
			SNew(SScaleBox)
			.Stretch(EStretch::ScaleToFill)
			[
				SNew(SImage).Image(BackgroundBrush.Get())
			]
		]
		+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom)
		.Padding(FMargin(0.0f, 0.0f, 0.0f, 48.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(FMargin(80.0f, 0.0f, 80.0f, 14.0f))
			[
				SAssignNew(TipText, STextBlock)
				.Text(FText::GetEmpty())
				.Font(TipFont)
				.ColorAndOpacity(FLinearColor(0.95f, 0.88f, 0.78f, 1.0f))
				.ShadowOffset(FVector2D(1.5f, 1.5f))
				.ShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.9f))
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
			+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
			[
				SNew(SBox).WidthOverride(1180.0f).HeightOverride(14.0f)
				[
					SAssignNew(ProgressBar, SProgressBar)
					.Percent(0.0f)
					.FillColorAndOpacity(FLinearColor::White)
				]
			]
		];

	Progress = 0.0f;
	HideCountdown = -1.0f;
	TipTimer = 0.0f;
	VisibleSeconds = 0.0f;
	if (TipOrder.IsEmpty())
	{
		RebuildTipOrder();
	}
	ShowNextTip();

	GameViewport->AddViewportWidgetContent(Root.ToSharedRef(), 10000);
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickLoading));
}

void USOTMLoadingScreenSubsystem::HideLoadingScreenAfter(const float Delay)
{
	if (Root.IsValid())
	{
		HideCountdown = FMath::Max(0.0f, Delay);
	}
}

bool USOTMLoadingScreenSubsystem::TickLoading(const float DeltaTime)
{
	using namespace SOTMLoadingScreenPrivate;
	if (!Root.IsValid())
	{
		return false;
	}
	// Ease towards 90% while loading; run out to 100% once a hide has been requested.
	Progress = FMath::FInterpTo(Progress, HideCountdown >= 0.0f ? 1.0f : 0.9f, DeltaTime, HideCountdown >= 0.0f ? 3.0f : 0.6f);
	if (ProgressBar.IsValid())
	{
		ProgressBar->SetPercent(Progress);
	}
	VisibleSeconds += DeltaTime;
	// Backstop: never leave the screen up forever if nothing asked to hide it.
	if (VisibleSeconds > 40.0f && HideCountdown < 0.0f)
	{
		HideCountdown = 0.5f;
	}
	TipTimer += DeltaTime;
	if (TipTimer >= TipInterval)
	{
		TipTimer = 0.0f;
		ShowNextTip();
	}
	if (HideCountdown >= 0.0f)
	{
		HideCountdown -= DeltaTime;
		if (HideCountdown <= 0.0f)
		{
			HideLoadingScreenNow();
			return false;
		}
	}
	return true;
}

void USOTMLoadingScreenSubsystem::HideLoadingScreenNow()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
	if (Root.IsValid())
	{
		if (UGameViewportClient* GameViewport = Viewport.Get())
		{
			GameViewport->RemoveViewportWidgetContent(Root.ToSharedRef());
		}
	}
	Root.Reset();
	TipText.Reset();
	ProgressBar.Reset();
	BackgroundBrush.Reset();
	BackgroundTexture = nullptr;
	HideCountdown = -1.0f;
}
