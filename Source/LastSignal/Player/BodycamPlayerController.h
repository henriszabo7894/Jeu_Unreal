#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "BodycamPlayerController.generated.h"

/**
 * Contrôleur du joueur. Applique la déformation "fish-eye" de l'objectif bodycam
 * (projection Panini) et limite l'angle de vue vertical.
 */
UCLASS()
class LASTSIGNAL_API ABodycamPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	/** Force de la déformation de l'objectif (0 = aucune, 1 = très forte). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bodycam")
	float LensDistortion = 0.65f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplyLensDistortion(float Strength);
};
