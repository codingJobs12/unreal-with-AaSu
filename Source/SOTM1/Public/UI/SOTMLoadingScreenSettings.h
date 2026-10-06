#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SOTMLoadingScreenSettings.generated.h"

class UTexture2D;

/**
 * Settings of the Chapter 1 loading screen (Project Settings > Game > SOTM Loading Screen).
 * The screen is shown by the engine's movie player, so it renders on its own thread while the
 * level loads and keeps animating even when the game thread is blocked.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="SOTM Loading Screen"))
class SOTM1_API USOTMLoadingScreenSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USOTMLoadingScreenSettings();

	virtual FName GetCategoryName() const override { return TEXT("Game"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	bool bEnabled = true;

	/** The screen is shown when the level being opened has ANY of these texts in its path (case-insensitive). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	TArray<FString> LevelPathContains;

	/** Full-screen background. Keep it 16:9 (it is scaled to fill the screen). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	TSoftObjectPtr<UTexture2D> BackgroundTexture;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	FText Title;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	FText SubTitle;

	/** Tip lines; one is shown at a time and they change every TipSeconds. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	TArray<FString> Tips;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen", meta=(ClampMin="1.0"))
	float TipSeconds = 5.0f;

	/** The screen stays up at least this long (no flash when the level loads very quickly). */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen", meta=(ClampMin="0.0"))
	float MinimumDisplaySeconds = 3.0f;

	/** Raises r.Streaming.PoolSize at runtime (fixes CH1 sometimes loading washed-out / blurry with
	 * "TEXTURE STREAMING POOL OVER ... BUDGET"). Applied at game start and before every level load. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Texture Streaming")
	bool bRaiseTextureStreamingPool = true;

	/** Pool size in MB. Lower it if your GPU has little video memory. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Texture Streaming", meta=(ClampMin="256"))
	int32 TextureStreamingPoolMB = 3000;

	/** Progress bar / subtitle colour. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Loading Screen")
	FLinearColor AccentColor = FLinearColor(0.85f, 0.10f, 0.12f, 1.0f);
};
