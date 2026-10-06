#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SOTMLoadingScreenSubsystem.generated.h"

class UTexture2D;
struct FSlateBrush;
struct FWorldContext;
class SWidget;

/**
 * Chapter 1 loading screen. Hooks the engine's map-load delegates and, when the destination
 * level matches USOTMLoadingScreenSettings::LevelNameContains, hands a Slate widget (background,
 * title, tips, progress bar) to the engine's movie player. The movie player draws on its own
 * thread, so the screen stays visible and animated for the WHOLE blocking load, every time.
 */
UCLASS()
class SOTM1_API USOTMLoadingScreenSubsystem final : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void ApplyStreamingPool();
	bool TickReset(float DeltaTime);
	TSharedRef<SWidget> MakeScreenWidget(const FString& MapName);
	void ShowViewportFallback(const FString& MapName);
	void HideViewportFallback();
	bool TickFallback(float DeltaTime);
	void ClearMovieAttributes();

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> Background;

	TSharedPtr<FSlateBrush> BackgroundBrush;

	FDelegateHandle PreLoadHandle;
	FDelegateHandle PostLoadHandle;
	FTSTicker::FDelegateHandle ResetTickerHandle;
	double ResetDeadline = 0.0;
	bool bScreenRequested = false;

	// Editor / PIE has no movie player: the same screen is shown as a viewport widget instead.
	TSharedPtr<SWidget> FallbackWidget;
	TSharedPtr<bool> FallbackFinished;
	FTSTicker::FDelegateHandle FallbackTickerHandle;
	double FallbackShownAt = 0.0;
	bool bFallbackActive = false;
};
