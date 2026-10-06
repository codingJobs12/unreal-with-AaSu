#include "SOTMAttackInputSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSOTMAttackInput, Log, All);

bool USOTMAttackInputSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void USOTMAttackInputSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	BindInput();
}

void USOTMAttackInputSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimer);
	}
	UnbindInput();
	Super::Deinitialize();
}

void USOTMAttackInputSubsystem::BindInput()
{
	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	UEnhancedInputComponent* Input = PC ? Cast<UEnhancedInputComponent>(PC->InputComponent) : nullptr;
	ULocalPlayer* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer
		? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	if (!Input || !InputSubsystem)
	{
		if (World)
		{
			World->GetTimerManager().SetTimer(RetryTimer, this, &ThisClass::BindInput, 0.5f, false);
		}
		return;
	}
	if (BoundInput.IsValid())
	{
		return;
	}

	AttackAction = NewObject<UInputAction>(this, TEXT("IA_SOTM_AttackKey"));
	AttackAction->ValueType = EInputActionValueType::Boolean;
	AttackContext = NewObject<UInputMappingContext>(this, TEXT("IMC_SOTM_AttackKey"));
	AttackContext->MapKey(AttackAction, EKeys::U);
	AttackContext->MapKey(AttackAction, EKeys::X);
	InputSubsystem->AddMappingContext(AttackContext, 50);
	BindingHandle = Input->BindAction(AttackAction, ETriggerEvent::Started, this, &ThisClass::HandleAttackInput).GetHandle();
	BoundInput = Input;
	UE_LOG(LogSOTMAttackInput, Display, TEXT("Attack key input bound (U / X)."));
}

void USOTMAttackInputSubsystem::UnbindInput()
{
	if (UEnhancedInputComponent* Input = BoundInput.Get(); Input && BindingHandle != 0)
	{
		Input->RemoveBindingByHandle(BindingHandle);
	}
	if (UWorld* World = GetWorld())
	{
		APlayerController* PC = World->GetFirstPlayerController();
		ULocalPlayer* LP = PC ? PC->GetLocalPlayer() : nullptr;
		if (UEnhancedInputLocalPlayerSubsystem* Sub = LP ? LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr)
		{
			if (AttackContext)
			{
				Sub->RemoveMappingContext(AttackContext);
			}
		}
	}
	BoundInput.Reset();
	BindingHandle = 0;
}

void USOTMAttackInputSubsystem::HandleAttackInput()
{
#if !UE_BUILD_SHIPPING
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	const FString Msg = FString::Printf(TEXT("Attack key pressed | Pawn: %s | Class: %s | Bound listeners: %s"),
		Pawn ? *Pawn->GetName() : TEXT("NONE"),
		Pawn ? *Pawn->GetClass()->GetName() : TEXT("NONE"),
		OnAttackKeyPressed.IsBound() ? TEXT("yes") : TEXT("NO"));
	UE_LOG(LogSOTMAttackInput, Display, TEXT("%s"), *Msg);
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
	}
#endif
	OnAttackKeyPressed.Broadcast();
}
