#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct FInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire")
	FText DisplayName;

	/** Poids en kilogrammes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire")
	float Weight = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire")
	bool bIsObjective = false;
};

/**
 * Sac du joueur. Garde la liste des objets et leur poids.
 * Le ralentissement lié au poids est déjà prêt mais désactivé (bWeightAffectsSpeed = false) :
 * on l'activera quand on travaillera sur cette mécanique.
 */
UCLASS(ClassGroup = (LastSignal), meta = (BlueprintSpawnableComponent))
class LASTSIGNAL_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	UFUNCTION(BlueprintCallable, Category = "Inventaire")
	void AddItem(const FInventoryItem& Item);

	UFUNCTION(BlueprintCallable, Category = "Inventaire")
	bool RemoveItem(FName ItemId);

	UFUNCTION(BlueprintPure, Category = "Inventaire")
	bool HasItem(FName ItemId) const;

	/** Poids total porté (équipement de base + objets du sac), en kg. */
	UFUNCTION(BlueprintPure, Category = "Inventaire")
	float GetTotalWeight() const;

	/** Multiplicateur de vitesse lié au poids (1 = pas de ralentissement). */
	UFUNCTION(BlueprintPure, Category = "Inventaire")
	float GetSpeedMultiplier() const;

	const TArray<FInventoryItem>& GetItems() const { return Items; }

	/** Mettre à true pour que le poids du sac ralentisse le joueur. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire|Poids")
	bool bWeightAffectsSpeed = false;

	/** Poids de l'équipement que le joueur porte toujours (gilet, arme, radio...). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire|Poids")
	float BaseGearWeight = 6.f;

	/** En dessous de ce poids, aucun ralentissement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire|Poids")
	float ComfortableWeight = 10.f;

	/** À ce poids, le ralentissement est maximal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire|Poids")
	float MaxWeight = 35.f;

	/** Vitesse minimale quand on est surchargé (0.5 = moitié de la vitesse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventaire|Poids")
	float MinSpeedMultiplier = 0.5f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventaire")
	TArray<FInventoryItem> Items;
};
