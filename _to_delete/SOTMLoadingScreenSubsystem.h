#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SOTMLoadingScreenSubsystem.generated.h"

class SProgressBar;
class STextBlock;
class SWidget;
class UGameViewportClient;
class UTexture2D;
class UUserWidget;

/**
 * Chapter loading screen (CH1 artwork + progress bar + rotating tips). Lives on the
 * GameInstance and draws raw Slate into the game viewport, so it survives OpenLevel:
 * shown before leaving the Mansion, removed shortly after CH1 has begun play.
 * Tips are shown one after another in a shuffled order and never repeat until every
 * tip has been shown once.
 */
UCLASS()
class SOTM1_API USOTMLoadingScreenSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void ShowLoadingScreen();
	/** Finishes the progress bar and removes the screen after Delay seconds. */
	void HideLoadingScreenAfter(float Delay);
	void HideLoadingScreenNow();
	bool IsLoadingScreenVisible() const { return Root.IsValid(); }

private:
	bool TickLoading(float DeltaTime);
	// The Menu System Pro loading screen (WBP_LoadingScreenMenu, shown after pressing Play)
	// is a Blueprint we can't edit, so it is detected when it appears, collapsed, and
	// replaced by this screen.
	bool PollEngineLoadingWidget(float DeltaTime);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void HandlePreLoadMap(const FString& MapName);
	void ShowNextTip();
	void RebuildTipOrder();

	TSharedPtr<SWidget> Root;
	TSharedPtr<STextBlock> TipText;
	TSharedPtr<SProgressBar> ProgressBar;
	TSharedPtr<struct FSlateBrush> BackgroundBrush;
	TWeakObjectPtr<UGameViewportClient> Viewport;
	FTSTicker::FDelegateHandle TickHandle;
	FTSTicker::FDelegateHandle PollHandle;
	FDelegateHandle PostLoadMapHandle;
	FDelegateHandle PreLoadMapHandle;
	float VisibleSeconds = 0.0f;
	// Menu System Pro loading widgets already replaced (never re-trigger on the same one).
	TSet<TWeakObjectPtr<UUserWidget>> HandledEngineWidgets;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BackgroundTexture;

	TArray<int32> TipOrder;
	int32 TipCursor = 0;
	int32 LastTipIndex = -1;
	float TipTimer = 0.0f;
	float Progress = 0.0f;
	float HideCountdown = -1.0f;
};
