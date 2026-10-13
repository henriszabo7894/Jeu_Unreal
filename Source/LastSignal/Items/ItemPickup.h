#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/Interactable.h"
#include "ItemPickup.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Objet ramassable posé dans le monde.
 * Si bIsObjective est coché, il compte pour l'objectif de la map.
 * Tu peux en placer toi-même dans l'éditeur (Place Actors -> "ItemPickup") et régler ses propriétés.
 */
UCLASS()
class LASTSIGNAL_API AItemPickup : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AItemPickup();

	virtual void OnConstruction(const FTransform& Transform) override;

	// IInteractable
	virtual FText GetInteractionText_Implementation() const override;
	virtual void Interact_Implementation(ABodycamCharacter* User) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet")
	FName ItemId = TEXT("Objet");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet")
	FText DisplayName;

	/** Poids en kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet")
	float Weight = 1.f;

	/** Compte pour l'objectif de la map. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet")
	bool bIsObjective = false;

	/** Couleur du bloc provisoire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet|Apparence")
	FLinearColor Color = FLinearColor(0.4f, 0.4f, 0.4f);

	/** Taille du bloc provisoire (1 = 1 mètre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet|Apparence")
	FVector MeshScale = FVector(0.3f, 0.2f, 0.2f);

	/** Petit reflet lumineux pour aider à repérer l'objet dans le noir. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objet|Apparence")
	bool bGlint = false;

protected:
	virtual void BeginPlay() override;

	void ApplyVisuals();

	UPROPERTY(VisibleAnywhere, Category = "Objet")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Objet")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Objet")
	TObjectPtr<UPointLightComponent> GlintLight;
};
