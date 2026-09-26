#include "Ability/SOTMSkillTreeSettings.h"

USOTMSkillTreeSettings::USOTMSkillTreeSettings()
{
	// Demo placeholder data - two columns, matching the two abilities that currently
	// exist. Costs/text are testing values, not final balance. Hardcoded here (not a
	// Config-loaded array) so there is exactly one source of truth - see the header
	// comment for why. To add a third ability's column, add another block here.
	FSOTMSkillTreeAbilityDefinition SpeedBoost;
	SpeedBoost.AbilityId = TEXT("SpeedBoost");
	SpeedBoost.DisplayName = FText::FromString(TEXT("SPEED BOOST"));
	{
		FSOTMSkillTreeLevelDefinition Lv2;
		Lv2.Level = 2;
		Lv2.AbilityPointCost = 1;
		Lv2.DisplayName = FText::FromString(TEXT("SPEED BOOST II"));
		Lv2.Description = FText::FromString(TEXT("Shorter cooldown between uses."));
		Lv2.CooldownDeltaSeconds = -2.0f; // 10s -> 8s
		SpeedBoost.UpgradeLevels.Add(Lv2);

		FSOTMSkillTreeLevelDefinition Lv3;
		Lv3.Level = 3;
		Lv3.AbilityPointCost = 2;
		Lv3.DisplayName = FText::FromString(TEXT("SPEED BOOST III"));
		Lv3.Description = FText::FromString(TEXT("Longer boost duration."));
		Lv3.DurationDeltaSeconds = 0.5f; // 3s -> 3.5s
		SpeedBoost.UpgradeLevels.Add(Lv3);

		FSOTMSkillTreeLevelDefinition Lv4;
		Lv4.Level = 4;
		Lv4.AbilityPointCost = 3;
		Lv4.DisplayName = FText::FromString(TEXT("SPEED BOOST IV"));
		Lv4.Description = FText::FromString(TEXT("Even shorter cooldown between uses."));
		Lv4.CooldownDeltaSeconds = -1.0f; // 8s -> 7s
		SpeedBoost.UpgradeLevels.Add(Lv4);
	}
	Abilities.Add(SpeedBoost);

	FSOTMSkillTreeAbilityDefinition LightningThrow;
	LightningThrow.AbilityId = TEXT("LightningThrow");
	LightningThrow.DisplayName = FText::FromString(TEXT("LIGHTNING THROW"));
	{
		FSOTMSkillTreeLevelDefinition Lv2;
		Lv2.Level = 2;
		Lv2.AbilityPointCost = 1;
		Lv2.DisplayName = FText::FromString(TEXT("LIGHTNING THROW II"));
		Lv2.Description = FText::FromString(TEXT("Longer stun range."));
		Lv2.RangeDelta = 400.0f; // 2200 -> 2600 units (22m -> 26m)
		LightningThrow.UpgradeLevels.Add(Lv2);

		FSOTMSkillTreeLevelDefinition Lv3;
		Lv3.Level = 3;
		Lv3.AbilityPointCost = 2;
		Lv3.DisplayName = FText::FromString(TEXT("LIGHTNING THROW III"));
		Lv3.Description = FText::FromString(TEXT("Shorter cooldown between throws."));
		Lv3.CooldownDeltaSeconds = -1.5f; // 4s -> 2.5s
		LightningThrow.UpgradeLevels.Add(Lv3);

		FSOTMSkillTreeLevelDefinition Lv4;
		Lv4.Level = 4;
		Lv4.AbilityPointCost = 3;
		Lv4.DisplayName = FText::FromString(TEXT("LIGHTNING THROW IV"));
		Lv4.Description = FText::FromString(TEXT("Even longer stun range."));
		Lv4.RangeDelta = 300.0f; // 2600 -> 2900 units (26m -> 29m)
		LightningThrow.UpgradeLevels.Add(Lv4);
	}
	Abilities.Add(LightningThrow);
}

const FSOTMSkillTreeAbilityDefinition* USOTMSkillTreeSettings::FindAbility(const FName AbilityId) const
{
	return Abilities.FindByPredicate([AbilityId](const FSOTMSkillTreeAbilityDefinition& Ability)
	{
		return Ability.AbilityId == AbilityId;
	});
}

const FSOTMSkillTreeLevelDefinition* USOTMSkillTreeSettings::FindLevel(const FName AbilityId, const int32 Level) const
{
	const FSOTMSkillTreeAbilityDefinition* Ability = FindAbility(AbilityId);
	if (!Ability)
	{
		return nullptr;
	}
	return Ability->UpgradeLevels.FindByPredicate([Level](const FSOTMSkillTreeLevelDefinition& LevelDef)
	{
		return LevelDef.Level == Level;
	});
}
