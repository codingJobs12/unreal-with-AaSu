#include "Ability/SOTMTimmyUpgradeStation.h"

#include "Ability/SOTMPhase3Settings.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Pawn.h"

ASOTMTimmyUpgradeStation::ASOTMTimmyUpgradeStation()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StationRoot"));
	SetRootComponent(SceneRoot);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(SceneRoot);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);

	StationTitle = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StationTitle"));
	StationTitle->SetupAttachment(SceneRoot);
	StationTitle->SetRelativeLocation(FVector(0.0f, 0.0f, 235.0f));
	StationTitle->SetHorizontalAlignment(EHTA_Center);
	StationTitle->SetVerticalAlignment(EVRTA_TextCenter);
	StationTitle->SetWorldSize(28.0f);
	StationTitle->SetText(FText::FromString(TEXT("TIMMY'S UPGRADE STATION")));
	StationTitle->SetTextRenderColor(FColor(186, 78, 255));
	StationTitle->SetCastShadow(false);
}

void ASOTMTimmyUpgradeStation::BeginPlay()
{
	Super::BeginPlay();
	InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleEndOverlap);

	// This actor is now placed (as a BP subclass) directly in the level, and a level
	// designer adjusts InteractionSphere's radius per-instance in the Editor. Forcing it
	// back to USOTMPhase3Settings::StationInteractionRadius here would silently undo that
	// hand-tuned value every time the level starts, so BeginPlay() no longer touches it -
	// whatever radius is set on the placed instance (class default or per-instance
	// override) is what is used.

	// NOTE: this actor is placed directly in the level now, so BeginPlay() runs as part
	// of normal level startup - before USOTMDemoPhase3WorldSubsystem has bound to
	// OnPlayerEntered/OnPlayerExited (it does that ~0.9s later). A broadcast from here
	// would reach zero listeners, so it deliberately does not attempt one. See
	// NotifyBoundListenersOfExistingOverlaps(), which the world subsystem calls right
	// after binding instead.
}

void ASOTMTimmyUpgradeStation::NotifyBoundListenersOfExistingOverlaps()
{
	// A normal Unreal overlap event only fires on the transition into/out of overlap -
	// an actor that was already inside the sphere before anyone was listening never
	// generates one. Call this once, right after binding OnPlayerEntered/OnPlayerExited,
	// to catch that case.
	TArray<AActor*> AlreadyOverlapping;
	InteractionSphere->GetOverlappingActors(AlreadyOverlapping, APawn::StaticClass());
	for (AActor* Overlapping : AlreadyOverlapping)
	{
		if (IsTargetPlayerActor(Overlapping))
		{
			OnPlayerEntered.Broadcast(Overlapping);
		}
	}
}

bool ASOTMTimmyUpgradeStation::IsTargetPlayerActor(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const UClass* ActorClass = Actor->GetClass();
	return ActorClass && ActorClass->GetName().Contains(TEXT("BP_MenuSystemCharacter"));
}

void ASOTMTimmyUpgradeStation::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;
	if (IsTargetPlayerActor(OtherActor))
	{
		OnPlayerEntered.Broadcast(OtherActor);
	}
}

void ASOTMTimmyUpgradeStation::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	if (IsTargetPlayerActor(OtherActor))
	{
		OnPlayerExited.Broadcast(OtherActor);
	}
}
