#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SOTMAttackInputSubsystem.generated.h"

class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSOTMAttackKeyPressedSignature);

/**
 * Input bridge only - contains NO combat logic. Maps U and X through Enhanced Input (the same
 * way F / Q / T already work in this project) and fires OnAttackKeyPressed so a Blueprint
 * (e.g. BP_MainCharacter) can bind to it: BeginPlay -> Get World Subsystem
 * (SOTMAttackInputSubsystem) -> Assign On Attack Key Pressed.
 */
UCLASS()
class SOTM1_API USOTMAttackInputSubsystem final : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Input")
	FSOTMAttackKeyPressedSignature OnAttackKeyPressed;

private:
	void BindInput();
	void UnbindInput();
	void HandleAttackInput();

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> AttackContext;

	TWeakObjectPtr<UEnhancedInputComponent> BoundInput;
	uint32 BindingHandle = 0;
	FTimerHandle RetryTimer;
};
