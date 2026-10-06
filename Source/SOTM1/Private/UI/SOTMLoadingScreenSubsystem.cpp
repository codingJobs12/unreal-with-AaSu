#include "UI/SOTMLoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UnrealClient.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "Fonts/SlateFontInfo.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "MoviePlayer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"
#include "UI/SOTMLoadingScreenSettings.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SSpacer.h"
#include "Styling/SlateColor.h"
#include "Widgets/Layout/SSpacer.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMLoadingScreen, Log, All);

namespace SOTMLoadingScreenPrivate
{
	/**
	 * The widget the movie player draws. It only reads plain data (copied texts, a brush that the
	 * subsystem keeps alive) and the clock, so it is safe to render from the loading thread.
	 */
	class SSOTMLoadingScreen final : public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SSOTMLoadingScreen) {}
			SLATE_ARGUMENT(TSharedPtr<FSlateBrush>, BackgroundBrush)
			SLATE_ARGUMENT(FText, Title)
			SLATE_ARGUMENT(FText, SubTitle)
			SLATE_ARGUMENT(TArray<FText>, Tips)
			SLATE_ARGUMENT(float, TipSeconds)
			SLATE_ARGUMENT(FString, MapPackage)
			SLATE_ARGUMENT(FLinearColor, Accent)
			SLATE_ARGUMENT(TSharedPtr<bool>, FinishedFlag)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs)
		{
			Brush = InArgs._BackgroundBrush;
			Tips = InArgs._Tips;
			TipSeconds = FMath::Max(InArgs._TipSeconds, 1.0f);
			MapPackage = InArgs._MapPackage;
			FinishedFlag = InArgs._FinishedFlag;
			StartTime = FPlatformTime::Seconds();
			const FLinearColor Accent = InArgs._Accent;
			BarFill = FSlateColorBrush(FLinearColor::White);
			BarFrame = FSlateColorBrush(FLinearColor::White);
			BarBack = FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f));
			Dark = FSlateColorBrush(FLinearColor(0.02f, 0.02f, 0.025f, 1.0f));
			Shade = FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.12f));
			(void)Accent;

			TSharedRef<SWidget> Background = SNullWidget::NullWidget;
			if (Brush.IsValid() && Brush->GetResourceObject())
			{
				Background = SNew(SScaleBox)
					.Stretch(EStretch::ScaleToFill)
					[
						SNew(SImage).Image(Brush.Get())
					];
			}

			// Optional title lines (the Chapter 1 artwork already carries its own title, so these default to empty).
			TSharedRef<SVerticalBox> TitleBox = SNew(SVerticalBox);
			if (!InArgs._Title.IsEmpty())
			{
				TitleBox->AddSlot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(InArgs._Title)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 54))
					.ColorAndOpacity(FLinearColor::White)
				];
			}
			if (!InArgs._SubTitle.IsEmpty())
			{
				TitleBox->AddSlot().AutoHeight().HAlign(HAlign_Center)
				[
					SNew(STextBlock).Text(InArgs._SubTitle)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 22))
					.ColorAndOpacity(Accent)
				];
			}

			// Layout like the reference: full-bleed artwork, tip line, then a long outlined progress bar at the bottom.
			ChildSlot
			[
				SNew(SOverlay)
				+ SOverlay::Slot()[ SNew(SBorder).BorderImage(&Dark) ]
				+ SOverlay::Slot()[ Background ]
				+ SOverlay::Slot()[ SNew(SBorder).BorderImage(&Shade) ]
				+ SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[ TitleBox ]
				+ SOverlay::Slot().HAlign(HAlign_Fill).VAlign(VAlign_Bottom).Padding(FMargin(0.0f, 0.0f, 0.0f, 34.0f))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(40.0f, 0.0f, 40.0f, 14.0f)
					[
						SNew(STextBlock)
						.Text_Lambda([this]() { return CurrentTip(); })
						.Font(FCoreStyle::GetDefaultFontStyle("Italic", 20))
						.ColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.92f))
						.ShadowOffset(FVector2D(1.0f, 1.0f))
						.ShadowColorAndOpacity(FLinearColor(0, 0, 0, 0.9f))
						.Justification(ETextJustify::Center)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(100.0f, 0.0f, 100.0f, 0.0f))
					[
						SNew(SBox).HeightOverride(16.0f)
						[
							SNew(SBorder).BorderImage(&BarFrame).Padding(1.0f)
							[
								SNew(SOverlay)
								+ SOverlay::Slot()[ SNew(SBorder).BorderImage(&BarBack) ]
								+ SOverlay::Slot()
								[
									SNew(SHorizontalBox)
									+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]() { return FMath::Max(Progress(), 0.001f); }))
									[
										SNew(SBorder).BorderImage(&BarFill)
									]
									+ SHorizontalBox::Slot().FillWidth(TAttribute<float>::CreateLambda([this]() { return FMath::Max(1.0f - Progress(), 0.001f); }))
									[
										SNew(SSpacer)
									]
								]
							]
						]
					]
				]
			];
		}

	private:
		double Elapsed() const { return FPlatformTime::Seconds() - StartTime; }

		FText CurrentTip() const
		{
			if (Tips.Num() == 0)
			{
				return FText::GetEmpty();
			}
			const int32 Index = static_cast<int32>(Elapsed() / TipSeconds) % Tips.Num();
			return Tips[Index];
		}

		/** Real async-load progress when the engine reports it, otherwise a smooth time curve; never goes backwards. */
		float Progress() const
		{
			if ((FinishedFlag.IsValid() && *FinishedFlag) || (GetMoviePlayer() && GetMoviePlayer()->IsLoadingFinished()))
			{
				Shown = 1.0f;
				return 1.0f;
			}
			float Estimate = 0.92f * (1.0f - FMath::Exp(-static_cast<float>(Elapsed()) / 4.0f));
			if (!MapPackage.IsEmpty())
			{
				const float Real = GetAsyncLoadPercentage(FName(*MapPackage));
				if (Real >= 0.0f)
				{
					Estimate = FMath::Max(Estimate, FMath::Clamp(Real / 100.0f, 0.0f, 1.0f) * 0.95f);
				}
			}
			Shown = FMath::Max(Shown, Estimate);
			return Shown;
		}

		FText LoadingLabel() const
		{
			const int32 Dots = static_cast<int32>(Elapsed() * 2.5) % 4;
			return FText::FromString(FString(TEXT("LOADING")) + FString::ChrN(Dots, TEXT('.')));
		}

		FText PercentText() const
		{
			return FText::FromString(FString::Printf(TEXT("%d%%"), FMath::Clamp(FMath::RoundToInt(Progress() * 100.0f), 0, 100)));
		}

		TSharedPtr<FSlateBrush> Brush;
		TArray<FText> Tips;
		float TipSeconds = 5.0f;
		FString MapPackage;
		double StartTime = 0.0;
		mutable float Shown = 0.0f;
		TSharedPtr<bool> FinishedFlag;
		FSlateColorBrush BarFrame = FSlateColorBrush(FLinearColor::White);
		FSlateColorBrush BarFill = FSlateColorBrush(FLinearColor::White);
		FSlateColorBrush BarBack = FSlateColorBrush(FLinearColor::White);
		FSlateColorBrush Dark = FSlateColorBrush(FLinearColor::Black);
		FSlateColorBrush Shade = FSlateColorBrush(FLinearColor::Black);
	};
}

bool USOTMLoadingScreenSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return !IsRunningDedicatedServer() && !IsRunningCommandlet() && Super::ShouldCreateSubsystem(Outer);
}

void USOTMLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const USOTMLoadingScreenSettings* Settings = GetDefault<USOTMLoadingScreenSettings>();
	if (!Settings || !Settings->bEnabled)
	{
		return;
	}
	// Load the background now (menu time) so nothing has to be loaded while the screen is up.
	if (UTexture2D* Texture = Settings->BackgroundTexture.LoadSynchronous())
	{
		Background = Texture;
		BackgroundBrush = MakeShared<FSlateBrush>();
		BackgroundBrush->SetResourceObject(Texture);
		BackgroundBrush->ImageSize = FVector2f(
			FMath::Max<float>(Texture->GetSizeX(), 1.0f), FMath::Max<float>(Texture->GetSizeY(), 1.0f));
		BackgroundBrush->DrawAs = ESlateBrushDrawType::Image;
	}
	else
	{
		UE_LOG(LogSOTMLoadingScreen, Warning, TEXT("Background texture '%s' not found - using a dark background."),
			*Settings->BackgroundTexture.ToString());
	}
	ApplyStreamingPool();
	PreLoadHandle = FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &USOTMLoadingScreenSubsystem::HandlePreLoadMap);
	PostLoadHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &USOTMLoadingScreenSubsystem::HandlePostLoadMap);
}

void USOTMLoadingScreenSubsystem::Deinitialize()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.Remove(PreLoadHandle);
	FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
	if (ResetTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ResetTickerHandle);
		ResetTickerHandle.Reset();
	}
	HideViewportFallback();
	BackgroundBrush.Reset();
	Super::Deinitialize();
}

TSharedRef<SWidget> USOTMLoadingScreenSubsystem::MakeScreenWidget(const FString& MapName)
{
	const USOTMLoadingScreenSettings* Settings = GetDefault<USOTMLoadingScreenSettings>();
	TArray<FText> TipTexts;
	for (const FString& Tip : Settings->Tips)
	{
		if (!Tip.IsEmpty())
		{
			TipTexts.Add(FText::FromString(Tip));
		}
	}
	FString Package = MapName;
	int32 OptionIndex = INDEX_NONE;
	if (Package.FindChar(TEXT('?'), OptionIndex))
	{
		Package.LeftInline(OptionIndex);
	}
	return SNew(SOTMLoadingScreenPrivate::SSOTMLoadingScreen)
		.BackgroundBrush(BackgroundBrush)
		.Title(Settings->Title)
		.SubTitle(Settings->SubTitle)
		.Tips(TipTexts)
		.TipSeconds(Settings->TipSeconds)
		.MapPackage(Package)
		.Accent(Settings->AccentColor)
		.FinishedFlag(FallbackFinished);
}

void USOTMLoadingScreenSubsystem::ApplyStreamingPool()
{
	// The texture streaming pool is tied to the scalability level (sg.TextureQuality) and can be far smaller than
	// the level's non-streaming textures, which makes CH1 load washed-out / blurry with "TEXTURE STREAMING POOL OVER
	// BUDGET". Raise it from code (SetByCode beats scalability, so it also survives scalability changes).
	const USOTMLoadingScreenSettings* Settings = GetDefault<USOTMLoadingScreenSettings>();
	if (!Settings || !Settings->bRaiseTextureStreamingPool)
	{
		return;
	}
	if (IConsoleVariable* PoolSize = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.PoolSize")))
	{
		if (PoolSize->GetInt() < Settings->TextureStreamingPoolMB)
		{
			PoolSize->Set(Settings->TextureStreamingPoolMB, ECVF_SetByCode);
			UE_LOG(LogSOTMLoadingScreen, Display, TEXT("r.Streaming.PoolSize raised to %d MB."), Settings->TextureStreamingPoolMB);
		}
	}
	if (IConsoleVariable* LimitToVRAM = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Streaming.LimitPoolSizeToVRAM")))
	{
		LimitToVRAM->Set(0, ECVF_SetByCode);
	}
}

void USOTMLoadingScreenSubsystem::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	ApplyStreamingPool(); // before ANY level load, so the pool is big enough while its textures stream in
	if (Background)
	{
		Background->SetForceMipLevelsToBeResident(20.0f); // sharp loading artwork without permanently using pool
	}
	const USOTMLoadingScreenSettings* Settings = GetDefault<USOTMLoadingScreenSettings>();
	if (!Settings || !Settings->bEnabled)
	{
		return;
	}
	bool bMatches = false;
	for (const FString& Filter : Settings->LevelPathContains)
	{
		if (!Filter.IsEmpty() && MapName.Contains(Filter, ESearchCase::IgnoreCase))
		{
			bMatches = true;
			break;
		}
	}
	UE_LOG(LogSOTMLoadingScreen, Display, TEXT("PreLoadMap '%s' -> loading screen %s."), *MapName, bMatches ? TEXT("YES") : TEXT("no (not a CH1 path)"));
	if (!bMatches)
	{
		return;
	}
	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	if (!MoviePlayer || !IsMoviePlayerEnabled())
	{
		// Editor / PIE: no movie player, so draw the same screen as a viewport widget.
		ShowViewportFallback(MapName);
		return;
	}

	// Remove any old Menu System Pro loading widget that is still on screen (it would sit under ours).
	for (TObjectIterator<UUserWidget> It; It; ++It)
	{
		UUserWidget* Widget = *It;
		if (IsValid(Widget) && !Widget->HasAnyFlags(RF_ClassDefaultObject) && Widget->IsInViewport()
			&& Widget->GetClass()->GetName().Contains(TEXT("LoadingScreenMenu")))
		{
			Widget->RemoveFromParent();
		}
	}

	FLoadingScreenAttributes Attributes;
	Attributes.bAutoCompleteWhenLoadingCompletes = true;   // closes itself when the level is ready
	Attributes.bMoviesAreSkippable = false;
	Attributes.bWaitForManualStop = false;
	Attributes.MinimumLoadingScreenDisplayTime = Settings->MinimumDisplaySeconds;
	Attributes.WidgetLoadingScreen = MakeScreenWidget(MapName);
	MoviePlayer->SetupLoadingScreen(Attributes);
	bScreenRequested = true;
	if (!MoviePlayer->IsMovieCurrentlyPlaying())
	{
		MoviePlayer->PlayMovie(); // the engine may already have started it - then this is skipped
	}
	UE_LOG(LogSOTMLoadingScreen, Display, TEXT("Loading screen (movie player) shown for %s."), *MapName);
}

void USOTMLoadingScreenSubsystem::ShowViewportFallback(const FString& MapName)
{
	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogSOTMLoadingScreen, Warning, TEXT("No game viewport - loading screen cannot be shown."));
		return;
	}
	HideViewportFallback();
	FallbackFinished = MakeShared<bool>(false);
	FallbackWidget = MakeScreenWidget(MapName);
	FallbackShownAt = FPlatformTime::Seconds();
	bFallbackActive = true;
	Viewport->AddViewportWidgetContent(FallbackWidget.ToSharedRef(), 100000);
	// The load below blocks the game thread, so draw one frame NOW: the last presented frame stays on screen.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().Tick();
	}
	if (Viewport->Viewport)
	{
		Viewport->Viewport->Draw();
	}
	UE_LOG(LogSOTMLoadingScreen, Display, TEXT("Loading screen (viewport fallback - editor/PIE) shown for %s."), *MapName);
}

void USOTMLoadingScreenSubsystem::HideViewportFallback()
{
	if (FallbackTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(FallbackTickerHandle);
		FallbackTickerHandle.Reset();
	}
	if (FallbackWidget.IsValid())
	{
		if (UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr)
		{
			Viewport->RemoveViewportWidgetContent(FallbackWidget.ToSharedRef());
		}
		FallbackWidget.Reset();
	}
	bFallbackActive = false;
}

bool USOTMLoadingScreenSubsystem::TickFallback(float DeltaTime)
{
	const USOTMLoadingScreenSettings* Settings = GetDefault<USOTMLoadingScreenSettings>();
	if (FPlatformTime::Seconds() - FallbackShownAt >= (Settings ? Settings->MinimumDisplaySeconds : 3.0f))
	{
		FallbackTickerHandle.Reset();
		HideViewportFallback();
		return false;
	}
	return true;
}

void USOTMLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (bFallbackActive)
	{
		// The level change removed the viewport widgets: put the screen back, full bar, until the minimum time is over.
		if (FallbackFinished.IsValid())
		{
			*FallbackFinished = true;
		}
		if (UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr)
		{
			if (FallbackWidget.IsValid())
			{
				Viewport->RemoveViewportWidgetContent(FallbackWidget.ToSharedRef());
				Viewport->AddViewportWidgetContent(FallbackWidget.ToSharedRef(), 100000);
			}
		}
		if (!FallbackTickerHandle.IsValid())
		{
			FallbackTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &USOTMLoadingScreenSubsystem::TickFallback), 0.1f);
		}
		return;
	}
	if (!bScreenRequested)
	{
		return;
	}
	// Once the screen has closed, clear the attributes so other levels do not get it.
	ResetDeadline = FPlatformTime::Seconds() + 60.0;
	if (!ResetTickerHandle.IsValid())
	{
		ResetTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &USOTMLoadingScreenSubsystem::TickReset), 0.25f);
	}
}

bool USOTMLoadingScreenSubsystem::TickReset(float DeltaTime)
{
	IGameMoviePlayer* MoviePlayer = GetMoviePlayer();
	if (!MoviePlayer || !MoviePlayer->IsMovieCurrentlyPlaying() || FPlatformTime::Seconds() > ResetDeadline)
	{
		ClearMovieAttributes();
		ResetTickerHandle.Reset();
		return false; // stop ticking
	}
	return true;
}

void USOTMLoadingScreenSubsystem::ClearMovieAttributes()
{
	if (IGameMoviePlayer* MoviePlayer = GetMoviePlayer())
	{
		MoviePlayer->SetupLoadingScreen(FLoadingScreenAttributes());
	}
	bScreenRequested = false;
}
