#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SOTMUIEffectsLibrary.generated.h"

class UWidget;

/**
 * Small reusable UI "juice" effects, callable from any UMG Blueprint graph.
 *
 * Usage: in any widget's Blueprint graph (e.g. an Upgrade/Unlock button's
 * OnClicked event, right after the purchase/unlock succeeds), call
 * "Play UI Unlock Shake" and pass in whichever widget should visibly react -
 * the whole panel, a single icon, the button itself, etc. Works on ANY UMG
 * widget (Image, Button, Border, a whole CanvasPanel/VerticalBox...), so the
 * same node can be reused across every upgrade/unlock screen in the project.
 */
UCLASS()
class SOTM1_API USOTMUIEffectsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Nudges TargetWidget down-and-right by ShakeOffset (in local/render
	 * transform units), then eases it back to its normal position over
	 * ShakeDuration seconds. Purely a RenderTransform animation - does not
	 * touch layout, so it never affects other widgets around it.
	 *
	 * Safe to call again on a widget that is still mid-shake; the new call
	 * simply restarts the effect from the current position instead of
	 * stacking timers.
	 */
	UFUNCTION(BlueprintCallable, Category = "SOTM|UI", meta = (WorldContext = "WorldContextObject"))
	static void PlayUIUnlockShake(
		UObject* WorldContextObject,
		UWidget* TargetWidget,
		float ShakeOffset = 14.0f,
		float ShakeDuration = 0.22f);

	/**
	 * Pushes Isabel's current/max health to the BossProgressBar on the in-game HUD.
	 * Call this from BP_IsabelBoss's own "Event Any Damage" graph, right after you
	 * subtract the damage from your Health variable - pass in your Health and
	 * MaxHealth variables directly. No component or interface setup needed on her
	 * Blueprint; this finds the HUD widget itself and updates the bar.
	 *
	 * Passing CurrentHealth <= 0 also hides the boss panel (same as on her death).
	 */
	UFUNCTION(BlueprintCallable, Category = "SOTM|UI", meta = (WorldContext = "WorldContextObject"))
	static void ReportIsabelBossHealth(UObject* WorldContextObject, float CurrentHealth, float MaxHealth);
};
