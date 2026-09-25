#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SOTMLightningThrowSettings.generated.h"

/** Chapter 1 second ability tuning. Lightning Throw stuns Cousins; it never kills them. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="SOTM Lightning Throw"))
class SOTM1_API USOTMLightningThrowSettings final : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Purchase", meta=(ClampMin="1"))
	int32 LightningThrowUnlockCost = 5; // TEMP: testing cost, was 60

	/** Design doc calls for a short cooldown, shorter than the Speed Boost. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="0.1", ClampMax="60.0"))
	float LightningThrowCooldown = 4.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="100.0", ClampMax="6000.0"))
	float LightningThrowRange = 2200.0f;

	/** Radius around the impact point that a Cousin must be within to be stunned. Kept for
	 *  back-compat/Blueprint use; the runtime targeting now uses the cone angle below. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="25.0", ClampMax="1500.0"))
	float LightningThrowHitRadius = 260.0f;

	/** Half-angle (degrees) of the forgiving aim cone in front of the player. A Cousin
	 *  anywhere inside this cone (and in range, and visible) can be stunned, not just one
	 *  standing exactly dead-centre of the crosshair. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Throw", meta=(ClampMin="1.0", ClampMax="90.0"))
	float LightningThrowConeHalfAngleDegrees = 25.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Stun", meta=(ClampMin="0.5", ClampMax="60.0"))
	float StunDuration = 5.0f;

	/** "Stunning one cousin makes the others angry" - others inside this radius aggro onto the player. */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Stun", meta=(ClampMin="0.0", ClampMax="20000.0"))
	float AggravationRadius = 3500.0f;
};
