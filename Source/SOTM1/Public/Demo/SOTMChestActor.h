#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SOTMChestActor.generated.h"

class UStaticMeshComponent;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSOTMChestOverlapSignature, AActor*, OtherActor);

/**
 * Native, self-contained Chest actor: a StaticMeshComponent (root) + a SphereComponent used
 * as its interaction trigger. Blueprintable so a BP_Chest can be made from this - assign the
 * mesh and tweak the sphere radius/collision there - while all gameplay logic stays in C++.
 */
UCLASS(Blueprintable)
class SOTM1_API ASOTMChestActor : public AActor
{
	GENERATED_BODY()

public:
	ASOTMChestActor();

	UFUNCTION(BlueprintPure, Category="SOTM|Chest")
	UStaticMeshComponent* GetChestMesh() const { return ChestMesh; }

	UFUNCTION(BlueprintPure, Category="SOTM|Chest")
	USphereComponent* GetInteractionSphere() const { return InteractionSphere; }

	// Broadcast when a Pawn enters/exits the interaction sphere - wire gameplay reactions
	// (prompts, open logic, etc.) to these rather than to the sphere's own overlap events.
	UPROPERTY(BlueprintAssignable, Category="SOTM|Chest")
	FSOTMChestOverlapSignature OnPlayerEntered;

	UPROPERTY(BlueprintAssignable, Category="SOTM|Chest")
	FSOTMChestOverlapSignature OnPlayerExited;

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, Category="SOTM|Chest")
	TObjectPtr<UStaticMeshComponent> ChestMesh;

	UPROPERTY(VisibleAnywhere, Category="SOTM|Chest")
	TObjectPtr<USphereComponent> InteractionSphere;
};
