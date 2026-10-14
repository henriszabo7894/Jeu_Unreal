#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoneBuilder.generated.h"

class UInstancedStaticMeshComponent;
class UPointLightComponent;
class USpotLightComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UTextRenderComponent;
class UMaterialInterface;

/** "Matériaux" (couleurs) utilisés par le blockout. */
enum class EZoneMat : uint8
{
	Ground,
	Asphalt,
	Sidewalk,
	Concrete,
	ConcreteDark,
	Brick,
	Plaster,
	Rust,
	Wood,
	CarRed,
	CarBlue,
	CarWhite,
	Burnt,
	Glass,
	Fabric,
	Line,
	Sandbag,
	Rubber,
	Count
};

/**
 * Génère toute la map "Zone de quarantaine - Secteur 4" à partir du code :
 * une rue en ruine de nuit, entourée d'un mur de quarantaine, avec immeubles (certains explorables),
 * voitures abandonnées, lampadaires, arbres morts, débris, feux de barils et un check-point militaire
 * qui sert de point d'extraction.
 *
 * C'est un "blockout" : des formes simples pour valider le level design et le gameplay.
 * Plus tard on remplacera les blocs par de vrais assets (Fab / Quixel Megascans).
 *
 * Change "Seed" dans l'éditeur pour obtenir une variante différente de la rue.
 */
UCLASS()
class LASTSIGNAL_API AZoneBuilder : public AActor
{
	GENERATED_BODY()

public:
	AZoneBuilder();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	FTransform GetPlayerStartTransform() const;

	/** Graine aléatoire : change-la pour une autre disposition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	int32 Seed = 1347;

	/** Longueur de la rue en cm (12000 = 120 m). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone", meta = (ClampMin = "6000", ClampMax = "30000"))
	float StreetLength = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	int32 NumCars = 11;

	// Décoche ces cases au fur et à mesure que tu remplaces les blocs par de vrais assets.
	// Sans immeubles générés, place toi-même les objets à ramasser (ItemPickup, Is Objective).

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildQuarantineWalls = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildBuildings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildCars = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildStreetProps = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildCheckpoint = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Blocs")
	bool bBuildDebris = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
	int32 NumDebris = 160;

	/** Fait apparaître les objets à ramasser, les munitions et l'extraction au lancement du jeu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Gameplay")
	bool bSpawnGameplayActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Gameplay")
	int32 NumAmmoPickups = 4;

	/** Éteint le soleil, le ciel et le brouillard déjà présents dans la map pour garder la nuit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Ambiance")
	bool bOverrideLevelLighting = true;

	/** Lumière de la lune (lux). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Ambiance")
	float MoonIntensity = 0.5f;

	/** Plus c'est bas, plus la nuit est sombre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Ambiance")
	float ExposureBias = -0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Ambiance")
	float FogDensity = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone|Ambiance")
	FLinearColor FogColor = FLinearColor(0.035f, 0.042f, 0.05f);

protected:
	virtual void BeginPlay() override;

	// ---------- Composants ----------
	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> CubeISMs;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> CylinderISMs;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TArray<TObjectPtr<UPointLightComponent>> PointLights;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<USpotLightComponent> Floodlight;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UDirectionalLightComponent> Moon;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<USkyLightComponent> SkyLight;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UExponentialHeightFogComponent> Fog;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UTextRenderComponent> QuarantineSign;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UTextRenderComponent> CheckpointSign;

	UPROPERTY(VisibleAnywhere, Category = "Zone")
	TObjectPtr<UTextRenderComponent> Graffiti;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BaseMaterial;

private:
	enum class ELightMode : uint8 { Steady, Fire, Broken };

	struct FLightAnim
	{
		int32 LightIndex = INDEX_NONE;
		float BaseIntensity = 0.f;
		ELightMode Mode = ELightMode::Steady;
		float Seed = 0.f;
	};

	struct FWallOpening
	{
		float Offset;  // position le long du mur
		float Width;
		float Bottom;  // hauteur du bas de l'ouverture
		float Top;     // hauteur du haut de l'ouverture
	};

	void Rebuild();
	void ApplyMaterials();
	void ApplyAtmosphere();
	void BuildZone();
	void SpawnGameplayActors();
	void DisableOtherLevelLighting();

	// Morceaux de la map
	void BuildGroundAndStreet(FRandomStream& R);
	void BuildQuarantineWalls();
	void BuildBuildings(FRandomStream& R);
	void BuildBuilding(FRandomStream& R, const FVector& Origin, int32 Side, float Width, float Depth, int32 Floors, bool bRuined, bool bEnterable, EZoneMat WallMat);
	void BuildCars(FRandomStream& R);
	void BuildCar(const FVector& Location, float Yaw, EZoneMat Paint, bool bBurnt, bool bFlipped);
	void BuildStreetFurniture(FRandomStream& R);
	void BuildDebris(FRandomStream& R);
	void BuildCheckpoint(FRandomStream& R);
	void BuildBarrier(const FVector& Location, float Yaw);
	void BuildFireBarrel(const FVector& Location);
	void BuildDeadTree(FRandomStream& R, const FVector& Base);

	// Outils de construction
	void Box(EZoneMat M, const FVector& Center, const FVector& Size, const FRotator& Rot = FRotator::ZeroRotator);
	void BoxT(EZoneMat M, const FTransform& Parent, const FVector& LocalCenter, const FVector& Size, const FRotator& LocalRot = FRotator::ZeroRotator);
	void Cyl(EZoneMat M, const FVector& Center, float Diameter, float Height, const FRotator& Rot = FRotator::ZeroRotator);
	void CylT(EZoneMat M, const FTransform& Parent, const FVector& LocalCenter, float Diameter, float Height, const FRotator& LocalRot = FRotator::ZeroRotator);
	void Wall(EZoneMat M, const FVector& Start, bool bAlongX, float Length, float Height, float Thickness, TArray<FWallOpening> Openings);
	void JaggedWall(FRandomStream& R, EZoneMat M, const FVector& Start, bool bAlongX, float Length, float MaxHeight, float Thickness);
	void WallSegment(EZoneMat M, const FVector& Start, bool bAlongX, float A, float B, float Z0, float Z1, float Thickness);
	int32 AddLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius, ELightMode Mode, bool bShadows);

	static FLinearColor GetMatColor(EZoneMat M);

	TArray<FLightAnim> LightAnims;
	int32 NextLight = 0;

	/** Endroits à l'intérieur des bâtiments où poser des objets (espace local). */
	TArray<FVector> InteriorSpots;

	FVector PlayerStartLocal = FVector::ZeroVector;
	FVector ExtractionLocal = FVector::ZeroVector;
	bool bGraffitiPlaced = false;
};
