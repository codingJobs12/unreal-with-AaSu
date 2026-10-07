#pragma once

#include "CoreMinimal.h"
#include "Ability/SOTMSkillTreeTypes.h"
#include "Blueprint/UserWidget.h"
#include "SOTMSkillTreeWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class USOTMPlayerStateSubsystem;

/**
 * Skill tree screen. FIXED layout, built entirely in C++ (no separate node widget
 * class, no WBP asset needed): exactly 2 abilities x 3 upgrade levels each, always -
 *
 *              [ UNLOCK ABILITY ]
 *             /                  \
 *   SPEED BOOST Lv2      LIGHTNING THROW Lv2
 *         |                       |
 *   SPEED BOOST Lv3      LIGHTNING THROW Lv3
 *         |                       |
 *   SPEED BOOST Lv4      LIGHTNING THROW Lv4
 *
 * one shared root banner (RootBannerText, plain text - not a button) above two
 * columns, each column stacking that ability's Level 2 node, then Level 3, then
 * Level 4. This is NOT expandable/generic - there is no loop over abilities and no
 * reusable node widget type; a third ability or a further level would mean adding new
 * named widgets/handlers here by hand, same as everything else below.
 *
 * All backend logic (what's locked/unlocked, what a node costs, spending points) lives
 * on this class as BlueprintCallable/BlueprintPure functions, unchanged. Every widget
 * below is BindWidgetOptional, so a WBP subclass can still hand-place widgets with
 * these EXACT names to override the C++-built default (see BuildDefaultLayout) -
 *   PointsText, CloseButton, RootBannerText,
 *   SpeedBoostNode2Button, SpeedBoostNode2Text, SpeedBoostNode3Button, SpeedBoostNode3Text,
 *   SpeedBoostNode4Button, SpeedBoostNode4Text,
 *   LightningThrowNode2Button, LightningThrowNode2Text, LightningThrowNode3Button, LightningThrowNode3Text,
 *   LightningThrowNode4Button, LightningThrowNode4Text,
 *   ConfirmDimBackground, ConfirmPanel, ConfirmTitleText, ConfirmComparisonText,
 *   ConfirmCostText, ConfirmButton, ConfirmCancelButton
 * - but BuildDefaultLayout only runs when nothing already bound a root widget, so the
 * C++ default and a WBP override never fight each other. Until/unless such a WBP
 * exists, this class works with no WBP asset at all.
 */
UCLASS(Blueprintable)
class SOTM1_API USOTMSkillTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// --- Backend: queries -----------------------------------------------------
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	int32 GetAbilityPoints() const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	bool IsAbilityBaseUnlocked(FName AbilityId) const;

	// True once every ability listed in USOTMSkillTreeSettings has its base unlocked
	// (currently Speed Boost + Lightning Throw, i.e. "both"). No Level 2+ node becomes
	// buyable until this is true - see GetNodeState.
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	bool AreBothAbilitiesUnlocked() const;

	// Current bought level for AbilityId: 0 = base not owned, 1 = base owned no
	// upgrades, 2+ = highest upgrade level bought.
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	int32 GetAbilityCurrentLevel(FName AbilityId) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	ESOTMSkillNodeState GetNodeState(FName AbilityId, int32 Level) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	int32 GetNodeCost(FName AbilityId, int32 Level) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	FText GetNodeDisplayName(FName AbilityId, int32 Level) const;

	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	FText GetNodeDescription(FName AbilityId, int32 Level) const;

	// --- Backend: actions -------------------------------------------------------
	// Attempts to buy AbilityId's node at Level. Level 1 routes to the existing
	// coin-paid TryPurchaseSpeedBoost/TryPurchaseLightningThrow; Level 2+ routes to the
	// new Ability-Point-paid PlayerState::TryUnlockSkillUpgrade. Refreshes every node's
	// visual afterward regardless of outcome, so locked/afforded state stays in sync.
	// This performs the purchase immediately - for Level 2+, a node click goes through
	// HandleNodeClicked/OpenUpgradeConfirm first so the player sees the before/after
	// comparison and has to Confirm; this function itself does not show that panel.
	UFUNCTION(BlueprintCallable, Category="SOTM|Skill Tree")
	bool TryUnlockNode(FName AbilityId, int32 Level);

	// Re-pulls state from PlayerState/Settings and updates all 4 fixed nodes + the AP
	// counter. Call after TryUnlockNode, and after opening the screen.
	UFUNCTION(BlueprintCallable, Category="SOTM|Skill Tree")
	void RefreshAllNodes();

	// Entry point for a node button click. Level 1 buys immediately via TryUnlockNode
	// (no stats to compare - it's unlocking from nothing); Level 2+ opens the upgrade
	// confirmation panel instead of buying right away.
	UFUNCTION(BlueprintCallable, Category="SOTM|Skill Tree")
	void HandleNodeClicked(FName AbilityId, int32 Level);

	// Builds and shows the confirmation panel for a pending Level 2+ purchase, with a
	// before/after comparison of the ability's real gameplay stats. No-ops if the node
	// isn't actually Unlockable right now (e.g. stale click after AP changed).
	UFUNCTION(BlueprintCallable, Category="SOTM|Skill Tree")
	void OpenUpgradeConfirm(FName AbilityId, int32 Level);

	// Multi-line "STAT   before -> after" text for AbilityId's Level node, built from the
	// ability's current effective stats (PlayerState::GetEffective...) plus this level's
	// own delta (FSOTMSkillTreeLevelDefinition). Empty for Level 1 (nothing to compare).
	UFUNCTION(BlueprintPure, Category="SOTM|Skill Tree")
	FText GetNodeComparisonText(FName AbilityId, int32 Level) const;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	// Builds the fixed layout in the class comment above entirely in C++ and assigns
	// the BindWidgetOptional properties below. Only called from NativeOnInitialized
	// when nothing already bound a root widget, i.e. no WBP subclass laid this screen
	// out by hand - see the class comment.
	void BuildDefaultLayout();

	// Constructs one node's Button + centered Text (its only two visual pieces) and
	// parents them together. Used 4 times by BuildDefaultLayout, once per fixed node -
	// there is no loop, each call site names its own AbilityId/Level explicitly.
	UButton* BuildNodeButton(FName ButtonWidgetName, FName TextWidgetName, TObjectPtr<UTextBlock>& OutText);

	// Recolors/re-texts/enables one node's Button+Text from the backend's current
	// state for AbilityId/Level. Shared by RefreshAllNodes's 4 explicit calls.
	void RefreshNodeVisual(UButton* Button, UTextBlock* Text, FName AbilityId, int32 Level) const;

	void CloseUpgradeConfirm();

	UFUNCTION()
	void HandleCloseClicked();

	/** Plays NewAnimation in reverse (if the widget has it), then closes. */
	UFUNCTION()
	void HandleCloseAnimationFinished();

	void FinishClose();

	/** "NewAnimation" from the widget Blueprint - played forward on open (by the BP) and in reverse on close. */
	UPROPERTY(Transient, BlueprintReadOnly, Category="Animation", meta=(BindWidgetAnimOptional, AllowPrivateAccess="true"))
	TObjectPtr<class UWidgetAnimation> NewAnimation;

	bool bClosing = false;

	UFUNCTION()
	void HandleConfirmUpgradeClicked();

	UFUNCTION()
	void HandleCancelUpgradeClicked();

	// One dedicated no-arg handler per fixed node's Button, since UButton::OnClicked
	// carries no arguments - each simply forwards to HandleNodeClicked with its own
	// hardcoded AbilityId/Level.
	UFUNCTION()
	void HandleSpeedBoostNode2Clicked();

	UFUNCTION()
	void HandleSpeedBoostNode3Clicked();

	UFUNCTION()
	void HandleLightningThrowNode2Clicked();

	UFUNCTION()
	void HandleLightningThrowNode3Clicked();

	UFUNCTION()
	void HandleSpeedBoostNode4Clicked();

	UFUNCTION()
	void HandleLightningThrowNode4Clicked();

	UPROPERTY(Transient)
	TObjectPtr<USOTMPlayerStateSubsystem> PlayerState;

	// --- WBP-bindable widgets --------------------------------------------------
	// Every property below is BindWidgetOptional: place a widget with this EXACT name
	// in a WBP subclass's Designer and it wires in automatically instead of the
	// C++-built default (see the class comment). Any left unbound simply stays null
	// and that piece silently does nothing (e.g. no points counter text).
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> PointsText;

	// Single shared root banner above both node columns - plain status text, not a
	// button; the base-ability purchases themselves happen elsewhere
	// (USOTMUpgradeStationWidget).
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> RootBannerText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> CloseButton;

	// The 6 fixed nodes - exactly 2 abilities x 3 upgrade levels each, matching
	// USOTMSkillTreeSettings and the layout diagram above. Not expandable: a third
	// ability or a further level needs its own new named pair of properties here, plus
	// its own click handler and its own explicit calls in BuildDefaultLayout/
	// RefreshAllNodes - nothing here loops.
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> SpeedBoostNode2Button;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedBoostNode2Text;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> SpeedBoostNode3Button;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedBoostNode3Text;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> SpeedBoostNode4Button;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedBoostNode4Text;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> LightningThrowNode2Button;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningThrowNode2Text;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> LightningThrowNode3Button;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningThrowNode3Text;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> LightningThrowNode4Button;
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> LightningThrowNode4Text;

	// --- Upgrade confirmation panel (shown on top of everything above for Level 2+) ---
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> ConfirmDimBackground;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UBorder> ConfirmPanel;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmTitleText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmComparisonText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ConfirmCostText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> ConfirmCancelButton;

	// Which node the confirmation panel is currently showing - set by OpenUpgradeConfirm,
	// consumed (and cleared) by HandleConfirmUpgradeClicked.
	FName PendingAbilityId;
	int32 PendingLevel = 0;
};
