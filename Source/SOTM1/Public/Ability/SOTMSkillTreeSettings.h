#pragma once

#include "CoreMinimal.h"
#include "Ability/SOTMSkillTreeTypes.h"
#include "Engine/DeveloperSettings.h"
#include "SOTMSkillTreeSettings.generated.h"

/**
 * Data-driven skill tree definition. Placeholder demo costs/text - not final balance.
 * To add a third ability's column later: add one more entry to Abilities in the
 * constructor (SOTMSkillTreeSettings.cpp). Nothing in USOTMSkillTreeWidget needs to
 * change to pick it up.
 *
 * Deliberately NOT a Config property: this used to be ini-editable via Project
 * Settings, but a Config TArray gets merged/appended with whatever the constructor
 * already added rather than replaced, which silently duplicated/misaligned entries
 * across editor/preview sessions. Compiled-in C++ defaults are the single source of
 * truth now - edit the constructor directly to change costs/levels/text.
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="SOTM Skill Tree"))
class SOTM1_API USOTMSkillTreeSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USOTMSkillTreeSettings();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Skill Tree")
	TArray<FSOTMSkillTreeAbilityDefinition> Abilities;

	// Looks up one ability's column definition by id. Returns nullptr if AbilityId isn't
	// a recognised column (e.g. a typo, or a column that hasn't been added yet).
	const FSOTMSkillTreeAbilityDefinition* FindAbility(FName AbilityId) const;

	// Looks up a specific Level-2+ node's definition within an ability's column. Returns
	// nullptr for Level 1 (not represented here - see FSOTMSkillTreeAbilityDefinition) or
	// an out-of-range level.
	const FSOTMSkillTreeLevelDefinition* FindLevel(FName AbilityId, int32 Level) const;
};
