#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SOTMPhase3Settings.generated.h"

/** Central, editable Phase 3 Level-1 tuning. Values are demo defaults, not final balance approval. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="SOTM Phase 3 Speed Boost"))
class SOTM1_API USOTMPhase3Settings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Purchase", meta=(ClampMin="1"))
	int32 SpeedBoostUnlockCost = 5; // TEMP: testing cost, was 250

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Level 1", meta=(ClampMin="1.0", ClampMax="2.0"))
	float SpeedBoostMultiplier = 2.0f; // 2x normal walk speed while Speed Boost (Q) is active

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Level 1", meta=(ClampMin="0.1", ClampMax="30.0"))
	float SpeedBoostDuration = 3.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Level 1", meta=(ClampMin="0.1", ClampMax="60.0"))
	float SpeedBoostCooldown = 10.0f;

	/** Blocks the Shift test sprint (IA_Sprint) in the forest so only Speed Boost (Q) changes speed. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Testing")
	bool bDisableShiftTestSprint = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Station", meta=(ClampMin="100.0", ClampMax="1500.0"))
	float StationInteractionRadius = 475.0f;

	// Widget class USOTMDemoPhase3WorldSubsystem::OpenSkillTreeUI spawns for the T-key
	// skill tree screen. Point this at WBP_SkillTree (or any other
	// USOTMSkillTreeWidget subclass) to use that Designer-built layout instead of the
	// plain native class; left unset, OpenSkillTreeUI falls back to
	// USOTMSkillTreeWidget::StaticClass() itself.
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="UI")
	TSubclassOf<class USOTMSkillTreeWidget> SkillTreeWidgetClass;
};
