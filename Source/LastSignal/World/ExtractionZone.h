#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExtractionZone.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Point d'extraction : une zone au sol marquée par un fumigène rouge.
 * Quand le joueur y entre avec tous les objectifs, la partie est gagnée.
 */
UCLASS()
class LASTSIGNAL_API AExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	AExtractionZone();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void CheckOverlaps();

	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	TObjectPtr<UStaticMeshComponent> FlareMesh;

	UPROPERTY(VisibleAnywhere, Category = "Extraction")
	TObjectPtr<UPointLightComponent> FlareLight;

	UPROPERTY(EditAnywhere, Category = "Extraction")
	float FlareIntensity = 120.f;

private:
	float CheckTimer = 0.f;
};
