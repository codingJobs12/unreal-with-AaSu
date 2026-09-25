#include "Demo/SOTMChestActor.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

namespace SOTMChestActorPrivate
{
	constexpr float DefaultInteractionRadius = 250.0f;
}

ASOTMChestActor::ASOTMChestActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetCanBeDamaged(false);

	ChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestMesh"));
	SetRootComponent(ChestMesh);
	// Purely visual/prop by default - the SphereComponent below is what drives interaction.
	// Left as query-only collision so it doesn't physically block the player; adjust in the
	// Blueprint if the chest should also be solid.
	ChestMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ChestMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ChestMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(ChestMesh);
	InteractionSphere->SetSphereRadius(SOTMChestActorPrivate::DefaultInteractionRadius);
	InteractionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionSphere->SetGenerateOverlapEvents(true);
	InteractionSphere->OnComponentBeginOverlap.AddUniqueDynamic(this, &ThisClass::HandleBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddUniqueDynamic(this, &ThisClass::HandleEndOverlap);
}

void ASOTMChestActor::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex,
	const bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;
	if (OtherActor && OtherActor->IsA<APawn>())
	{
		OnPlayerEntered.Broadcast(OtherActor);
	}
}

void ASOTMChestActor::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComponent,
	const int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComponent;
	(void)OtherBodyIndex;
	if (OtherActor && OtherActor->IsA<APawn>())
	{
		OnPlayerExited.Broadcast(OtherActor);
	}
}
