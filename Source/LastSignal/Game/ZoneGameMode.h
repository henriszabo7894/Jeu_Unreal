#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ZoneGameMode.generated.h"

class AItemPickup;
class ABodycamCharacter;
class AZoneBuilder;
class APlayerStart;

UENUM(BlueprintType)
enum class EZoneState : uint8
{
	Playing,
	Extracted,
	Dead
};

USTRUCT(BlueprintType)
struct FZoneObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Objectif")
	FName ItemId;

	UPROPERTY(BlueprintReadOnly, Category = "Objectif")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Objectif")
	bool bCollected = false;
};

/**
 * Règles de la partie :
 *  - les objets "objectif" de la map s'enregistrent ici
 *  - quand tout est ramassé, le joueur peut s'extraire au point d'extraction
 *  - si le joueur meurt ou s'extrait, la map redémarre après quelques secondes
 * Si la map ne contient pas de ZoneBuilder, il en crée un automatiquement.
 */
UCLASS()
class LASTSIGNAL_API AZoneGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AZoneGameMode();

	virtual void StartPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void RegisterObjective(AItemPickup* Pickup);
	void OnObjectiveCollected(AItemPickup* Pickup, ABodycamCharacter* Collector);
	void TryExtract(ABodycamCharacter* Character);
	void OnPlayerDied(ABodycamCharacter* Character);

	EZoneState GetZoneState() const { return ZoneState; }
	const TArray<FZoneObjective>& GetObjectives() const { return Objectives; }
	int32 GetCollectedCount() const;
	bool AreAllObjectivesCollected() const;
	float GetTimeSinceEnd() const;

	/** Crée la map automatiquement si aucun ZoneBuilder n'est présent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	bool bAutoSpawnZoneBuilder = true;

	/** Secondes avant de relancer la map après la fin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	float RestartDelay = 8.f;

protected:
	AZoneBuilder* EnsureZoneBuilder();
	void EndZone(EZoneState NewState, ABodycamCharacter* Character);
	void RestartZone();

	UPROPERTY(Transient)
	TArray<FZoneObjective> Objectives;

	UPROPERTY(Transient)
	TObjectPtr<AZoneBuilder> CachedBuilder;

	UPROPERTY(Transient)
	TObjectPtr<APlayerStart> RuntimePlayerStart;

	EZoneState ZoneState = EZoneState::Playing;
	float EndTime = 0.f;
	float LastExtractDeniedTime = -100.f;
	FTimerHandle RestartTimer;
};
