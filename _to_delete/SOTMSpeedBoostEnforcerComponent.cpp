#include "SOTMSpeedBoostEnforcerComponent.h"

#include "Demo/SOTMDemoPhase3WorldSubsystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

USOTMSpeedBoostEnforcerComponent::USOTMSpeedBoostEnforcerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the owning Pawn's own Tick (and therefore after its Blueprint Sprint logic has
	// already had its say on MaxWalkSpeed this frame) - see the class comment for why that
	// ordering is exactly what makes the boost stick.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void USOTMSpeedBoostEnforcerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		Movement = Character->GetCharacterMovement();
	}
}

void USOTMSpeedBoostEnforcerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Movement)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const USOTMDemoPhase3WorldSubsystem* Phase3 = World
		? World->GetSubsystem<USOTMDemoPhase3WorldSubsystem>() : nullptr;
	if (!Phase3)
	{
		return;
	}

	const float BoostedSpeed = Phase3->GetActiveBoostedSpeedOrZero();
	if (BoostedSpeed > 0.0f)
	{
		// Speed Boost is Active this frame - unconditionally win over whatever the
		// Blueprint's own Sprint tick set MaxWalkSpeed to earlier this frame.
		Movement->MaxWalkSpeed = BoostedSpeed;
	}
	// Otherwise leave MaxWalkSpeed alone entirely - Sprint and any other speed system the
	// Blueprint owns behaves exactly as it already does.
}
