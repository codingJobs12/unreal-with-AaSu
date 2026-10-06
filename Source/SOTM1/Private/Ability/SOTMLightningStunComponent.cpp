#include "Ability/SOTMLightningStunComponent.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "BrainComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Demo/SOTMBossDialogue.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

USOTMLightningStunComponent::USOTMLightningStunComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool USOTMLightningStunComponent::CanBeStunned() const
{
	const UWorld* World = GetWorld();
	return !bPermanentlyDisabled && !bStunned && World && World->GetTimeSeconds() >= ImmuneUntil;
}

bool USOTMLightningStunComponent::ApplyLightningStun(const float DefaultDuration)
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!World || !Pawn || !CanBeStunned())
	{
		return false;
	}
	const float Duration = FMath::Max(StunDurationOverride > 0.0f ? StunDurationOverride : DefaultDuration, 0.1f);
	bStunned = true;

	if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
	{
		AI->StopMovement();
		if (UBrainComponent* Brain = AI->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("LightningStun"));
		}
	}
	if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		Character->GetCharacterMovement()->StopMovementImmediately();
		Character->GetCharacterMovement()->DisableMovement();
		if (StunMontage && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			Character->GetMesh()->GetAnimInstance()->Montage_Play(StunMontage);
		}
	}
	if (!StunDialogueRow.IsNone())
	{
		if (USOTMBossDialogueSubsystem* Dialogue = World->GetSubsystem<USOTMBossDialogueSubsystem>())
		{
			Dialogue->PlayLine(StunDialogueRow);
		}
	}

	OnStunStarted.Broadcast(Duration);
	World->GetTimerManager().SetTimer(StunTimer, this, &ThisClass::EndStun, Duration, false);
	return true;
}

void USOTMLightningStunComponent::EndStun()
{
	if (!bStunned)
	{
		return;
	}
	bStunned = false;
	if (UWorld* World = GetWorld())
	{
		ImmuneUntil = World->GetTimeSeconds() + ImmunityAfterStun;
		World->GetTimerManager().ClearTimer(StunTimer);
	}
	if (bPermanentlyDisabled)
	{
		return;
	}
	if (APawn* Pawn = Cast<APawn>(GetOwner()))
	{
		if (ACharacter* Character = Cast<ACharacter>(Pawn))
		{
			if (StunMontage && Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
			{
				Character->GetMesh()->GetAnimInstance()->Montage_Stop(0.2f, StunMontage);
			}
			Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
		if (AAIController* AI = Cast<AAIController>(Pawn->GetController()))
		{
			if (UBrainComponent* Brain = AI->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
		}
	}
	OnStunEnded.Broadcast();
}

void USOTMLightningStunComponent::DisableStunPermanently()
{
	bPermanentlyDisabled = true;
	if (bStunned)
	{
		bStunned = false;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(StunTimer);
		}
	}
}

void USOTMLightningStunComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StunTimer);
	}
	Super::EndPlay(EndPlayReason);
}
