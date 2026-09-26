#pragma once

#include "CoreMinimal.h"
#include "SOTMSkillTreeTypes.generated.h"

/** Visual/interaction state of a single node in the skill tree UI. */
UENUM(BlueprintType)
enum class ESOTMSkillNodeState : uint8
{
	// Prerequisite not met yet (base ability not owned, or previous level not bought).
	Locked,
	// Prerequisite met, enough Ability Points on hand, and safe to press.
	Unlockable,
	// Prerequisite met, but not enough Ability Points to afford this node right now.
	NotEnoughPoints,
	// Already bought.
	Unlocked
};

/**
 * One purchasable rung of an ability's ladder, Level 2 and above. Level 1 is the base
 * ability unlock itself (already paid in coins via TryPurchaseSpeedBoost/
 * TryPurchaseLightningThrow) and is not represented here - the widget derives Level 1's
 * node purely from PlayerState's existing IsXUnlocked() query.
 */
USTRUCT(BlueprintType)
struct FSOTMSkillTreeLevelDefinition
{
	GENERATED_BODY()

	// 2, 3, 4... Must be sequential per ability, starting at 2, with no gaps.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(ClampMin="2"))
	int32 Level = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree", meta=(ClampMin="1"))
	int32 AbilityPointCost = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FText Description;

	// Real gameplay effect this node applies on top of the ability's current effective
	// stats (base + every lower level's own delta). Only the field(s) relevant to this
	// ability's flavor of upgrade should be non-zero - see USOTMSkillTreeSettings's
	// constructor for which ability uses which. Positive Duration/Range = better;
	// negative Cooldown = shorter (better).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree|Effect")
	float DurationDeltaSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree|Effect")
	float CooldownDeltaSeconds = 0.0f;

	// Units (cm), matching LightningThrowRange - not meters.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree|Effect")
	float RangeDelta = 0.0f;
};

/**
 * One column of the skill tree: a single ability's whole ladder. Adding a third ability
 * later is just adding one more entry to USOTMSkillTreeSettings::Abilities - the widget
 * loops over this array to build columns, so nothing about the UI itself needs to change.
 */
USTRUCT(BlueprintType)
struct FSOTMSkillTreeAbilityDefinition
{
	GENERATED_BODY()

	// Matches the AbilityId string the widget's backend and PlayerState use to look this
	// ability up, e.g. "SpeedBoost" or "LightningThrow".
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FName AbilityId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	FText DisplayName;

	// Level 1 (the base unlock) is not listed here - see the struct comment above.
	// Ordered starting at Level 2; the widget renders one node per entry, top to bottom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Skill Tree")
	TArray<FSOTMSkillTreeLevelDefinition> UpgradeLevels;
};
