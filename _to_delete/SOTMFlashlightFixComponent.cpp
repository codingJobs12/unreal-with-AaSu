#include "SOTMFlashlightFixComponent.h"

#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"

namespace SOTMFlashlightFixPrivate
{
	// Requested SpotLightComponent::SourceRadius.
	constexpr float DesiredSourceRadius = 200.0f;
	// Component variable name of the flashlight prop mesh on BP_MenuSystemCharacter.
	const TCHAR* FlashlightMeshComponentName = TEXT("Flashlight");
}

USOTMFlashlightFixComponent::USOTMFlashlightFixComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// After the owning Pawn's own Tick (normally TG_PrePhysics) and after animation
	// evaluation, so this always gets the final say on the SpotLight's transform for the
	// frame - see the class comment for why that is what actually kills the flicker.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void USOTMFlashlightFixComponent::BeginPlay()
{
	Super::BeginPlay();
	FindTargets();

	if (SpotLight)
	{
		SpotLight->SetSourceRadius(SOTMFlashlightFixPrivate::DesiredSourceRadius);
	}

	if (SpotLight && FlashlightMesh && SpotLight->GetAttachParent() != FlashlightMesh)
	{
		// Rigidly parent it under the flashlight mesh (which already correctly tracks the
		// hand) instead of leaving it positioned independently. KeepWorldTransform means it
		// keeps aiming wherever it currently visually aims at the moment of attachment - no
		// risk of suddenly pointing the wrong way.
		SpotLight->AttachToComponent(FlashlightMesh, FAttachmentTransformRules::KeepWorldTransform);
	}
}

void USOTMFlashlightFixComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!SpotLight || !FlashlightMesh)
	{
		return;
	}

	// Hard-snap every frame, after everything else this frame (including any leftover
	// Blueprint Tick logic that also repositions the SpotLight from a socket read) has
	// already run. This is the unconditional fix: whatever position the Blueprint computed
	// earlier in the frame gets overwritten here with the flashlight mesh's current, fully
	// up-to-date transform right before the frame renders.
	SpotLight->SetWorldLocationAndRotation(
		FlashlightMesh->GetComponentLocation(),
		FlashlightMesh->GetComponentRotation());
}

void USOTMFlashlightFixComponent::FindTargets()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!SpotLight)
	{
		SpotLight = Owner->FindComponentByClass<USpotLightComponent>();
	}
	if (!FlashlightMesh)
	{
		for (UActorComponent* Component : Owner->GetComponents())
		{
			UStaticMeshComponent* StaticMeshComp = Cast<UStaticMeshComponent>(Component);
			if (StaticMeshComp && StaticMeshComp->GetName() == SOTMFlashlightFixPrivate::FlashlightMeshComponentName)
			{
				FlashlightMesh = StaticMeshComp;
				break;
			}
		}
	}

	if (!SpotLight || !FlashlightMesh)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("SOTMFlashlightFixComponent: could not find required components on %s (SpotLight=%s, %s mesh=%s) - flashlight fix inactive."),
			*GetNameSafe(Owner),
			SpotLight ? TEXT("found") : TEXT("missing"),
			SOTMFlashlightFixPrivate::FlashlightMeshComponentName,
			FlashlightMesh ? TEXT("found") : TEXT("missing"));
	}
}
