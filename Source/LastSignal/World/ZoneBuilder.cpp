#include "World/ZoneBuilder.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Templates/Function.h"
#include "UObject/ConstructorHelpers.h"

#include "Items/AmmoPickup.h"
#include "Items/ItemPickup.h"
#include "World/ExtractionZone.h"

// Toutes les distances sont en centimètres (unité d'Unreal).
namespace ZoneLayout
{
	constexpr float RoadHalfWidth = 600.f;     // route : 12 m de large
	constexpr float SidewalkOuter = 1000.f;    // trottoirs : 4 m
	constexpr float BuildingDepth = 1500.f;    // profondeur des immeubles
	constexpr float ZoneHalfWidth = 3000.f;    // mur de quarantaine à 30 m de l'axe de la rue
	constexpr float FloorHeight = 350.f;
	constexpr float WallThickness = 25.f;
	constexpr float WallHeight = 800.f;        // mur de quarantaine
	constexpr float StartX = -800.f;
	constexpr int32 NumPointLights = 16;
}

AZoneBuilder::AZoneBuilder()
{
	PrimaryActorTick.bCanEverTick = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		BaseMaterial = ShapeMaterial.Object;
	}

	// Un composant "instancié" par matériau et par forme : des milliers de blocs pour un coût minime
	for (int32 i = 0; i < static_cast<int32>(EZoneMat::Count); ++i)
	{
		UInstancedStaticMeshComponent* Cubes = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Cubes_%d"), i));
		Cubes->SetupAttachment(Root);
		if (CubeMesh.Succeeded())
		{
			Cubes->SetStaticMesh(CubeMesh.Object);
		}
		CubeISMs.Add(Cubes);

		UInstancedStaticMeshComponent* Cylinders = CreateDefaultSubobject<UInstancedStaticMeshComponent>(*FString::Printf(TEXT("Cylinders_%d"), i));
		Cylinders->SetupAttachment(Root);
		if (CylinderMesh.Succeeded())
		{
			Cylinders->SetStaticMesh(CylinderMesh.Object);
		}
		CylinderISMs.Add(Cylinders);
	}

	for (int32 i = 0; i < ZoneLayout::NumPointLights; ++i)
	{
		UPointLightComponent* Light = CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("PointLight_%d"), i));
		Light->SetupAttachment(Root);
		Light->SetMobility(EComponentMobility::Movable);
		Light->IntensityUnits = ELightUnits::Candelas;
		Light->SetVisibility(false);
		PointLights.Add(Light);
	}

	Floodlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Floodlight"));
	Floodlight->SetupAttachment(Root);
	Floodlight->SetMobility(EComponentMobility::Movable);
	Floodlight->IntensityUnits = ELightUnits::Candelas;
	Floodlight->Intensity = 9000.f;
	Floodlight->AttenuationRadius = 5000.f;
	Floodlight->InnerConeAngle = 12.f;
	Floodlight->OuterConeAngle = 24.f;
	Floodlight->LightColor = FColor(220, 235, 255);
	Floodlight->VolumetricScatteringIntensity = 2.f;

	Moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
	Moon->SetupAttachment(Root);
	Moon->SetMobility(EComponentMobility::Movable);
	Moon->SetRelativeRotation(FRotator(-38.f, 35.f, 0.f));
	Moon->LightColor = FColor(150, 170, 255);
	Moon->VolumetricScatteringIntensity = 1.5f;

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = 0.5f;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;

	QuarantineSign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("QuarantineSign"));
	QuarantineSign->SetupAttachment(Root);
	CheckpointSign = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CheckpointSign"));
	CheckpointSign->SetupAttachment(Root);
	Graffiti = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Graffiti"));
	Graffiti->SetupAttachment(Root);
	for (UTextRenderComponent* Text : { QuarantineSign.Get(), CheckpointSign.Get(), Graffiti.Get() })
	{
		Text->SetHorizontalAlignment(EHTA_Center);
		Text->SetVerticalAlignment(EVRTA_TextCenter);
		Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AZoneBuilder::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AZoneBuilder::BeginPlay()
{
	Super::BeginPlay();

	// On reconstruit au lancement : garantit que tout est à jour, même dans une version packagée
	Rebuild();

	if (bSpawnGameplayActors && HasAuthority())
	{
		SpawnGameplayActors();
	}
}

void AZoneBuilder::Rebuild()
{
	ApplyMaterials();
	ApplyAtmosphere();
	BuildZone();
}

FTransform AZoneBuilder::GetPlayerStartTransform() const
{
	const FTransform& T = GetActorTransform();
	return FTransform(T.GetRotation(), T.TransformPosition(PlayerStartLocal + FVector(0.f, 0.f, 100.f)));
}

// =====================================================================
//  Couleurs / ambiance
// =====================================================================

FLinearColor AZoneBuilder::GetMatColor(EZoneMat M)
{
	switch (M)
	{
	case EZoneMat::Ground:       return FLinearColor(0.045f, 0.043f, 0.04f);
	case EZoneMat::Asphalt:      return FLinearColor(0.03f, 0.03f, 0.032f);
	case EZoneMat::Sidewalk:     return FLinearColor(0.11f, 0.105f, 0.1f);
	case EZoneMat::Concrete:     return FLinearColor(0.16f, 0.155f, 0.145f);
	case EZoneMat::ConcreteDark: return FLinearColor(0.07f, 0.07f, 0.068f);
	case EZoneMat::Brick:        return FLinearColor(0.16f, 0.06f, 0.04f);
	case EZoneMat::Plaster:      return FLinearColor(0.2f, 0.18f, 0.14f);
	case EZoneMat::Rust:         return FLinearColor(0.12f, 0.05f, 0.025f);
	case EZoneMat::Wood:         return FLinearColor(0.1f, 0.065f, 0.035f);
	case EZoneMat::CarRed:       return FLinearColor(0.22f, 0.03f, 0.025f);
	case EZoneMat::CarBlue:      return FLinearColor(0.03f, 0.06f, 0.14f);
	case EZoneMat::CarWhite:     return FLinearColor(0.3f, 0.3f, 0.28f);
	case EZoneMat::Burnt:        return FLinearColor(0.015f, 0.013f, 0.012f);
	case EZoneMat::Glass:        return FLinearColor(0.02f, 0.025f, 0.03f);
	case EZoneMat::Fabric:       return FLinearColor(0.08f, 0.09f, 0.05f);
	case EZoneMat::Line:         return FLinearColor(0.35f, 0.33f, 0.25f);
	case EZoneMat::Sandbag:      return FLinearColor(0.17f, 0.14f, 0.09f);
	case EZoneMat::Rubber:       return FLinearColor(0.012f, 0.012f, 0.012f);
	default:                     return FLinearColor(0.1f, 0.1f, 0.1f);
	}
}

void AZoneBuilder::ApplyMaterials()
{
	if (!BaseMaterial)
	{
		return;
	}
	for (int32 i = 0; i < static_cast<int32>(EZoneMat::Count); ++i)
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		MID->SetFlags(RF_Transient);
		MID->SetVectorParameterValue(TEXT("Color"), GetMatColor(static_cast<EZoneMat>(i)));
		if (CubeISMs.IsValidIndex(i) && CubeISMs[i])
		{
			CubeISMs[i]->SetMaterial(0, MID);
		}
		if (CylinderISMs.IsValidIndex(i) && CylinderISMs[i])
		{
			CylinderISMs[i]->SetMaterial(0, MID);
		}
	}
}

void AZoneBuilder::ApplyAtmosphere()
{
	Moon->SetIntensity(MoonIntensity);

	Fog->SetFogDensity(FogDensity);
	Fog->SetFogHeightFalloff(0.25f);
	Fog->SetFogInscatteringColor(FogColor);
	Fog->SetStartDistance(0.f);
	Fog->SetVolumetricFog(true);

	FPostProcessSettings& S = PostProcess->Settings;
	// Exposition : on garde la nuit vraiment sombre
	S.bOverride_AutoExposureMethod = true;
	S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = -4.f;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = 2.f;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = ExposureBias;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = 1.5f;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = 1.f;
	// Image désaturée, légèrement verdâtre, contrastée : ambiance "found footage"
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(0.62f, 0.66f, 0.66f, 1.f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.12f, 1.12f, 1.12f, 1.f);
	S.bOverride_ColorGain = true;
	S.ColorGain = FVector4(0.96f, 1.f, 0.97f, 1.f);
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.8f;
}

// =====================================================================
//  Construction de la map
// =====================================================================

void AZoneBuilder::BuildZone()
{
	for (UInstancedStaticMeshComponent* ISM : CubeISMs)
	{
		if (ISM) ISM->ClearInstances();
	}
	for (UInstancedStaticMeshComponent* ISM : CylinderISMs)
	{
		if (ISM) ISM->ClearInstances();
	}
	for (UPointLightComponent* Light : PointLights)
	{
		if (Light) Light->SetVisibility(false);
	}
	LightAnims.Reset();
	NextLight = 0;
	InteriorSpots.Reset();
	bGraffitiPlaced = false;
	Graffiti->SetVisibility(false);

	PlayerStartLocal = FVector(250.f, -250.f, 0.f);
	ExtractionLocal = FVector(StreetLength - 350.f, 0.f, 0.f);

	FRandomStream R(Seed);
	BuildGroundAndStreet(R);
	BuildQuarantineWalls();
	BuildBuildings(R);
	BuildCars(R);
	BuildStreetFurniture(R);
	BuildCheckpoint(R);
	BuildDebris(R);
}

void AZoneBuilder::BuildGroundAndStreet(FRandomStream& R)
{
	using namespace ZoneLayout;
	const float L = StreetLength;
	const float XMax = L + 600.f;
	const float Len = XMax - StartX;
	const float MidX = (StartX + XMax) * 0.5f;

	// Sol (dessus à z = 0)
	Box(EZoneMat::Ground, FVector(MidX, 0.f, -25.f), FVector(Len + 400.f, ZoneHalfWidth * 2.f + 400.f, 50.f));
	// Chaussée (dessus à z = 1)
	Box(EZoneMat::Asphalt, FVector(MidX, 0.f, -24.f), FVector(Len, RoadHalfWidth * 2.f, 50.f));
	// Trottoirs (dessus à z = 15)
	for (int32 Side : { 1, -1 })
	{
		Box(EZoneMat::Sidewalk, FVector(MidX, Side * (RoadHalfWidth + SidewalkOuter) * 0.5f, -10.f),
			FVector(Len, SidewalkOuter - RoadHalfWidth, 50.f));
	}

	// Ligne centrale en pointillés, effacée par endroits
	for (float X = 0.f; X < L - 1600.f; X += 600.f)
	{
		if (R.FRand() < 0.2f) continue;
		Box(EZoneMat::Line, FVector(X + 150.f, 0.f, 1.5f), FVector(300.f, 14.f, 1.f));
	}

	// Traces de brûlé et taches d'huile
	for (int32 i = 0; i < 18; ++i)
	{
		const FVector Pos(R.FRandRange(0.f, L - 1000.f), R.FRandRange(-500.f, 500.f), 1.2f);
		Box(EZoneMat::Burnt, Pos, FVector(R.FRandRange(150.f, 500.f), R.FRandRange(100.f, 300.f), 1.f), FRotator(0.f, R.FRandRange(0.f, 180.f), 0.f));
	}
}

void AZoneBuilder::BuildQuarantineWalls()
{
	using namespace ZoneLayout;
	const float XMax = StreetLength + 600.f;
	const float Len = XMax - StartX;
	const float MidX = (StartX + XMax) * 0.5f;
	const float Z = WallHeight * 0.5f;

	// Mur de béton tout autour du quartier
	Box(EZoneMat::ConcreteDark, FVector(MidX, ZoneHalfWidth + 50.f, Z), FVector(Len + 200.f, 100.f, WallHeight));
	Box(EZoneMat::ConcreteDark, FVector(MidX, -ZoneHalfWidth - 50.f, Z), FVector(Len + 200.f, 100.f, WallHeight));
	Box(EZoneMat::ConcreteDark, FVector(StartX - 50.f, 0.f, Z), FVector(100.f, ZoneHalfWidth * 2.f + 200.f, WallHeight));
	Box(EZoneMat::ConcreteDark, FVector(XMax + 50.f, 0.f, Z), FVector(100.f, ZoneHalfWidth * 2.f + 200.f, WallHeight));

	// Poteaux de barbelés sur le haut des murs
	for (float X = StartX; X < XMax; X += 400.f)
	{
		for (int32 Side : { 1, -1 })
		{
			Cyl(EZoneMat::Rust, FVector(X, Side * (ZoneHalfWidth + 50.f), WallHeight + 60.f), 8.f, 120.f);
		}
	}

	// Panneau au départ, sur le mur derrière le joueur
	QuarantineSign->SetRelativeLocation(FVector(StartX + 2.f, 0.f, 430.f));
	QuarantineSign->SetRelativeRotation(FRotator::ZeroRotator);
	QuarantineSign->SetText(NSLOCTEXT("LastSignal", "QuarantineSign", "ZONE DE QUARANTAINE - SECTEUR 4<br>ACCÈS INTERDIT - TIR À VUE"));
	QuarantineSign->SetWorldSize(55.f);
	QuarantineSign->SetTextRenderColor(FColor(200, 30, 20));
}

void AZoneBuilder::BuildBuildings(FRandomStream& R)
{
	using namespace ZoneLayout;
	const float EndX = StreetLength - 2300.f; // on laisse de la place au check-point

	for (int32 Side : { 1, -1 })
	{
		float X = 0.f;
		int32 Index = 0;
		while (X < EndX - 900.f)
		{
			const float Width = FMath::Min(R.FRandRange(1100.f, 1900.f), EndX - X);
			if (Width < 900.f)
			{
				break;
			}
			const int32 Floors = R.RandRange(1, 3);
			const bool bRuined = R.FRand() < 0.35f;
			const bool bEnterable = (Index % 2 == 0) || R.FRand() < 0.5f;
			const EZoneMat Mats[] = { EZoneMat::Brick, EZoneMat::Plaster, EZoneMat::Concrete };
			const EZoneMat WallMat = Mats[R.RandRange(0, 2)];

			BuildBuilding(R, FVector(X, Side * SidewalkOuter, 0.f), Side, Width, BuildingDepth, Floors, bRuined, bEnterable, WallMat);

			const float Alley = R.FRandRange(350.f, 650.f);
			// Benne à ordures dans la ruelle
			if (R.FRand() < 0.5f)
			{
				const FVector DumpsterPos(X + Width + Alley * 0.5f, Side * (SidewalkOuter + 500.f), 65.f);
				Box(EZoneMat::Rust, DumpsterPos, FVector(130.f, 200.f, 130.f), FRotator(0.f, R.FRandRange(-15.f, 15.f), 0.f));
			}

			X += Width + Alley;
			++Index;
		}
	}
}

void AZoneBuilder::BuildBuilding(FRandomStream& R, const FVector& O, int32 Side, float W, float D, int32 Floors, bool bRuined, bool bEnterable, EZoneMat M)
{
	using namespace ZoneLayout;
	const float FH = FloorHeight;
	const float T = WallThickness;
	const float H = Floors * FH;
	const float FrontY = O.Y + Side * T * 0.5f;
	const float BackY = O.Y + Side * (D - T * 0.5f);
	const float MinY = FMath::Min(O.Y, O.Y + Side * D);
	const float CenterY = O.Y + Side * D * 0.5f;

	for (int32 F = 0; F < Floors; ++F)
	{
		const float Z0 = F * FH;
		const bool bJagged = bRuined && F > 0 && F == Floors - 1;

		// --- Façade (côté rue) ---
		TArray<FWallOpening> Front;
		if (F == 0)
		{
			Front.Add({ W * 0.18f - 60.f, 120.f, 100.f, 220.f });
			if (bEnterable)
			{
				Front.Add({ W * 0.5f - 70.f, 140.f, 0.f, 230.f });
			}
			Front.Add({ W * 0.78f - 60.f, 120.f, 100.f, 220.f });
		}
		else
		{
			Front.Add({ W * 0.15f - 60.f, 120.f, 100.f, 230.f });
			Front.Add({ W * 0.45f - 60.f, 120.f, 100.f, 230.f });
			Front.Add({ W * 0.75f - 60.f, 120.f, 100.f, 230.f });
		}

		// --- Arrière ---
		TArray<FWallOpening> Back;
		if (F == 0 && bEnterable)
		{
			Back.Add({ W * 0.3f - 60.f, 120.f, 0.f, 220.f });
		}
		else
		{
			Back.Add({ W * 0.3f - 60.f, 120.f, 100.f, 220.f });
		}
		Back.Add({ W * 0.7f - 60.f, 120.f, 100.f, 220.f });

		// --- Côtés ---
		TArray<FWallOpening> Sides;
		if (F > 0)
		{
			Sides.Add({ D * 0.5f - 60.f, 120.f, 100.f, 230.f });
		}

		const FVector FrontStart(O.X, FrontY, Z0);
		const FVector BackStart(O.X, BackY, Z0);
		const FVector LeftStart(O.X + T * 0.5f, MinY, Z0);
		const FVector RightStart(O.X + W - T * 0.5f, MinY, Z0);

		if (bJagged)
		{
			JaggedWall(R, M, FrontStart, true, W, FH, T);
			JaggedWall(R, M, BackStart, true, W, FH, T);
			JaggedWall(R, M, LeftStart, false, D, FH, T);
			JaggedWall(R, M, RightStart, false, D, FH, T);
		}
		else
		{
			Wall(M, FrontStart, true, W, FH, T, Front);
			Wall(M, BackStart, true, W, FH, T, Back);
			Wall(M, LeftStart, false, D, FH, T, Sides);
			Wall(M, RightStart, false, D, FH, T, Sides);
		}

		// Fenêtres condamnées par des planches au rez-de-chaussée
		if (F == 0)
		{
			for (const FWallOpening& Op : Front)
			{
				if (Op.Bottom > 0.f && R.FRand() < 0.5f)
				{
					const float CX = O.X + Op.Offset + Op.Width * 0.5f;
					for (int32 p = 0; p < 3; ++p)
					{
						const float PZ = R.FRandRange(Op.Bottom + 15.f, Op.Top - 15.f);
						Box(EZoneMat::Wood, FVector(CX, O.Y - Side * 6.f, PZ), FVector(Op.Width + 30.f, 4.f, 16.f), FRotator(R.FRandRange(-14.f, 14.f), 0.f, 0.f));
					}
				}
			}
		}

		// Plancher entre les étages
		if (F > 0)
		{
			Box(EZoneMat::ConcreteDark, FVector(O.X + W * 0.5f, CenterY, Z0), FVector(W - 2.f * T, D - 2.f * T, 20.f));
		}
	}

	// Sol intérieur (dessus à z = 4)
	Box(bEnterable ? EZoneMat::Wood : EZoneMat::ConcreteDark, FVector(O.X + W * 0.5f, CenterY, 2.f), FVector(W - 2.f * T, D - 2.f * T, 4.f));

	// Toit
	if (!bRuined)
	{
		Box(EZoneMat::ConcreteDark, FVector(O.X + W * 0.5f, CenterY, H + 10.f), FVector(W + 20.f, D + 20.f, 20.f));
		// Petit muret sur le toit
		Box(M, FVector(O.X + W * 0.5f, O.Y, H + 50.f), FVector(W, 20.f, 60.f));
	}
	else if (Floors == 1)
	{
		// Toit à moitié effondré : la lune passe par le trou
		Box(EZoneMat::ConcreteDark, FVector(O.X + W * 0.225f, CenterY, H + 10.f), FVector(W * 0.45f, D, 20.f));
		if (!bEnterable)
		{
			Box(EZoneMat::ConcreteDark, FVector(O.X + W * 0.65f, CenterY, FH * 0.45f), FVector(W * 0.45f, D * 0.6f, 20.f), FRotator(28.f, 0.f, 0.f));
		}
	}

	// Gravats devant les bâtiments en ruine
	if (bRuined)
	{
		for (int32 i = 0; i < 14; ++i)
		{
			const FVector Pos(O.X + R.FRandRange(0.f, W), O.Y - Side * R.FRandRange(20.f, 250.f), 25.f);
			const float S = R.FRandRange(25.f, 90.f);
			Box(R.FRand() < 0.5f ? M : EZoneMat::ConcreteDark, Pos, FVector(S, S * R.FRandRange(0.6f, 1.4f), S * 0.6f),
				FRotator(R.FRandRange(-25.f, 25.f), R.FRandRange(0.f, 180.f), R.FRandRange(-25.f, 25.f)));
		}
	}

	// Intérieur explorable : mobilier et emplacements pour les objets
	if (bEnterable)
	{
		const FVector TablePos(O.X + W * 0.35f, O.Y + Side * D * 0.4f, 44.f);
		Box(EZoneMat::Wood, TablePos, FVector(160.f, 90.f, 80.f), FRotator(0.f, R.FRandRange(-10.f, 10.f), 0.f));
		InteriorSpots.Add(FVector(TablePos.X, TablePos.Y, 84.f));

		// Étagère contre le mur du fond
		const FVector ShelfPos(O.X + W * 0.62f, BackY - Side * 40.f, 104.f);
		Box(EZoneMat::Wood, ShelfPos, FVector(W * 0.28f, 40.f, 200.f));
		InteriorSpots.Add(FVector(ShelfPos.X, BackY - Side * 120.f, 4.f));

		// Matelas au sol, chaise renversée
		Box(EZoneMat::Fabric, FVector(O.X + W * 0.82f, O.Y + Side * D * 0.6f, 14.f), FVector(190.f, 95.f, 20.f), FRotator(0.f, R.FRandRange(-20.f, 20.f), 0.f));
		Box(EZoneMat::Wood, FVector(O.X + W * 0.25f, O.Y + Side * D * 0.62f, 25.f), FVector(45.f, 45.f, 45.f), FRotator(R.FRandRange(-80.f, 80.f), R.FRandRange(0.f, 90.f), 0.f));

		InteriorSpots.Add(FVector(O.X + 90.f, O.Y + Side * (D - 130.f), 4.f));

		// Néon mourant au plafond
		if (R.FRand() < 0.35f)
		{
			AddLight(FVector(O.X + W * 0.5f, CenterY, FH - 30.f), FLinearColor(0.75f, 0.95f, 1.f), 10.f, 900.f, ELightMode::Broken, true);
		}
	}

	// Graffiti sur la façade du premier immeuble rencontré
	if (!bGraffitiPlaced && Side == 1)
	{
		bGraffitiPlaced = true;
		Graffiti->SetVisibility(true);
		Graffiti->SetRelativeLocation(FVector(O.X + W * 0.5f, O.Y - 2.f, 300.f));
		Graffiti->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));
		Graffiti->SetText(NSLOCTEXT("LastSignal", "Graffiti", "ILS ENTENDENT TOUT"));
		Graffiti->SetWorldSize(42.f);
		Graffiti->SetTextRenderColor(FColor(120, 10, 8));
	}
}

void AZoneBuilder::BuildCars(FRandomStream& R)
{
	using namespace ZoneLayout;
	const EZoneMat Paints[] = { EZoneMat::CarRed, EZoneMat::CarBlue, EZoneMat::CarWhite, EZoneMat::ConcreteDark };

	// Carambolage contre le mur de départ : on ne peut pas revenir en arrière
	BuildCar(FVector(StartX + 300.f, -350.f, 0.f), 80.f, EZoneMat::CarWhite, true, false);
	BuildCar(FVector(StartX + 320.f, 250.f, 0.f), 105.f, EZoneMat::CarBlue, false, true);

	// Voitures abandonnées le long de la rue
	float X = 1300.f;
	for (int32 i = 0; i < NumCars && X < StreetLength - 2200.f; ++i)
	{
		const bool bAcross = R.FRand() < 0.2f;
		float Yaw = (R.FRand() < 0.5f ? 0.f : 180.f) + R.FRandRange(-25.f, 25.f);
		if (bAcross)
		{
			Yaw = 90.f + R.FRandRange(-20.f, 20.f);
		}
		const float Y = bAcross ? R.FRandRange(-300.f, 300.f) : (R.FRand() < 0.5f ? 1.f : -1.f) * R.FRandRange(150.f, 430.f);
		const bool bBurnt = R.FRand() < 0.3f;
		const bool bFlipped = R.FRand() < 0.12f;
		BuildCar(FVector(X, Y, 0.f), Yaw, Paints[R.RandRange(0, 3)], bBurnt, bFlipped);
		X += R.FRandRange(700.f, 1300.f);
	}
}

void AZoneBuilder::BuildCar(const FVector& Location, float Yaw, EZoneMat Paint, bool bBurnt, bool bFlipped)
{
	const FTransform CarT(FRotator(0.f, Yaw, bFlipped ? 180.f : 0.f), Location + FVector(0.f, 0.f, bFlipped ? 150.f : 0.f));
	const EZoneMat Body = bBurnt ? EZoneMat::Burnt : Paint;
	const EZoneMat Windows = bBurnt ? EZoneMat::Burnt : EZoneMat::Glass;
	const EZoneMat Tires = bBurnt ? EZoneMat::Burnt : EZoneMat::Rubber;

	BoxT(Body, CarT, FVector(0.f, 0.f, 55.f), FVector(450.f, 185.f, 65.f));      // carrosserie
	BoxT(Windows, CarT, FVector(-30.f, 0.f, 112.f), FVector(255.f, 168.f, 50.f)); // vitres
	BoxT(Body, CarT, FVector(-30.f, 0.f, 140.f), FVector(240.f, 165.f, 8.f));     // toit
	for (int32 SX : { 1, -1 })
	{
		for (int32 SY : { 1, -1 })
		{
			CylT(Tires, CarT, FVector(SX * 145.f, SY * 85.f, 35.f), 70.f, 28.f, FRotator(0.f, 0.f, 90.f));
		}
	}
}

void AZoneBuilder::BuildStreetFurniture(FRandomStream& R)
{
	using namespace ZoneLayout;
	const float LampY = SidewalkOuter - 80.f;

	for (int32 Side : { 1, -1 })
	{
		for (float X = 600.f; X < StreetLength - 1800.f; X += 1800.f)
		{
			const float LX = X + (Side > 0 ? 0.f : 900.f);
			if (R.FRand() < 0.25f)
			{
				// Lampadaire tombé sur la chaussée
				Cyl(EZoneMat::Rust, FVector(LX, Side * (LampY - 300.f), 22.f), 15.f, 650.f, FRotator(90.f, 90.f + R.FRandRange(-20.f, 20.f), 0.f));
			}
			else
			{
				Cyl(EZoneMat::Rust, FVector(LX, Side * LampY, 15.f + 325.f), 15.f, 650.f);
				Box(EZoneMat::Rust, FVector(LX, Side * (LampY - 90.f), 655.f), FVector(15.f, 180.f, 10.f));
				Box(EZoneMat::ConcreteDark, FVector(LX, Side * (LampY - 175.f), 645.f), FVector(45.f, 70.f, 18.f));
				if (R.FRand() < 0.35f)
				{
					// Quelques lampadaires grésillent encore
					AddLight(FVector(LX, Side * (LampY - 175.f), 620.f), FLinearColor(1.f, 0.55f, 0.25f), 70.f, 1800.f, ELightMode::Broken, true);
				}
			}

			// Arbre mort entre deux lampadaires
			if (R.FRand() < 0.7f)
			{
				BuildDeadTree(R, FVector(LX + 900.f * (Side > 0 ? 1.f : -1.f) * 0.5f, Side * (LampY - 20.f), 15.f));
			}
		}
	}

	// Sacs mortuaires sur le trottoir
	for (int32 i = 0; i < 5; ++i)
	{
		const int32 Side = R.FRand() < 0.5f ? 1 : -1;
		const FVector Pos(R.FRandRange(1500.f, StreetLength - 2500.f), Side * R.FRandRange(680.f, 900.f), 28.f);
		Box(EZoneMat::Burnt, Pos, FVector(185.f, 60.f, 26.f), FRotator(0.f, R.FRandRange(-30.f, 30.f), 0.f));
	}

	// Barils en feu au milieu de la rue
	BuildFireBarrel(FVector(StreetLength * 0.3f, 380.f, 0.f));
	BuildFireBarrel(FVector(StreetLength * 0.55f, -420.f, 0.f));
}

void AZoneBuilder::BuildDeadTree(FRandomStream& R, const FVector& Base)
{
	const float TrunkH = R.FRandRange(320.f, 480.f);
	Cyl(EZoneMat::Wood, Base + FVector(0.f, 0.f, TrunkH * 0.5f), 32.f, TrunkH, FRotator(R.FRandRange(-4.f, 4.f), 0.f, R.FRandRange(-4.f, 4.f)));
	for (int32 b = 0; b < 4; ++b)
	{
		const FRotator BranchRot(R.FRandRange(25.f, 60.f), R.FRandRange(0.f, 360.f), 0.f);
		const float BranchLen = R.FRandRange(120.f, 220.f);
		const FVector Attach = Base + FVector(0.f, 0.f, TrunkH * R.FRandRange(0.6f, 0.95f));
		const FVector Dir = BranchRot.RotateVector(FVector::UpVector);
		Cyl(EZoneMat::Wood, Attach + Dir * BranchLen * 0.5f, 10.f, BranchLen, BranchRot);
	}
}

void AZoneBuilder::BuildFireBarrel(const FVector& Location)
{
	Cyl(EZoneMat::Rust, Location + FVector(0.f, 0.f, 45.f), 60.f, 90.f);
	AddLight(Location + FVector(0.f, 0.f, 130.f), FLinearColor(1.f, 0.42f, 0.12f), 60.f, 1100.f, ELightMode::Fire, true);
}

void AZoneBuilder::BuildBarrier(const FVector& Location, float Yaw)
{
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location);
	BoxT(EZoneMat::Concrete, T, FVector(0.f, 0.f, 25.f), FVector(300.f, 60.f, 50.f));
	BoxT(EZoneMat::Concrete, T, FVector(0.f, 0.f, 65.f), FVector(300.f, 30.f, 40.f));
}

void AZoneBuilder::BuildCheckpoint(FRandomStream& R)
{
	using namespace ZoneLayout;
	const float CX = StreetLength - 1500.f;

	// Ligne de barrières en béton en travers de la rue, avec un passage au milieu
	for (float Y : { -1150.f, -850.f, -550.f, 550.f, 850.f, 1150.f })
	{
		const bool bOnSidewalk = FMath::Abs(Y) > RoadHalfWidth && FMath::Abs(Y) < SidewalkOuter;
		BuildBarrier(FVector(CX, Y, bOnSidewalk ? 15.f : 0.f), 90.f);
	}
	// Barrière levée au passage
	Box(EZoneMat::Line, FVector(CX + 20.f, -330.f, 100.f), FVector(10.f, 10.f, 200.f));
	Box(EZoneMat::Line, FVector(CX + 20.f, -230.f, 300.f), FVector(10.f, 10.f, 220.f), FRotator(0.f, 0.f, -25.f));

	// Murets de sacs de sable
	for (int32 Side : { 1, -1 })
	{
		for (int32 i = 0; i < 6; ++i)
		{
			for (int32 Layer = 0; Layer < 3; ++Layer)
			{
				const float Offset = (Layer % 2) * 30.f;
				Box(EZoneMat::Sandbag, FVector(CX + 180.f, Side * (330.f + i * 60.f + Offset), 11.f + Layer * 21.f),
					FVector(40.f, 58.f, 22.f), FRotator(0.f, R.FRandRange(-6.f, 6.f), 0.f));
			}
		}
	}

	// Guérite
	const FVector BoothPos(CX + 300.f, -750.f, 15.f);
	Box(EZoneMat::Concrete, BoothPos + FVector(0.f, 0.f, 60.f), FVector(200.f, 200.f, 120.f));
	Box(EZoneMat::Glass, BoothPos + FVector(0.f, 0.f, 165.f), FVector(200.f, 200.f, 90.f));
	Box(EZoneMat::ConcreteDark, BoothPos + FVector(0.f, 0.f, 220.f), FVector(230.f, 230.f, 20.f));
	CheckpointSign->SetRelativeLocation(BoothPos + FVector(-102.f, 0.f, 60.f));
	CheckpointSign->SetRelativeRotation(FRotator(0.f, 180.f, 0.f));
	CheckpointSign->SetText(NSLOCTEXT("LastSignal", "CheckpointSign", "POINT DE CONTRÔLE<br>EXTRACTION"));
	CheckpointSign->SetWorldSize(22.f);
	CheckpointSign->SetTextRenderColor(FColor(210, 210, 200));

	// Tentes militaires
	for (const FVector& TentPos : { FVector(CX + 750.f, 800.f, 15.f), FVector(CX + 750.f, -850.f, 15.f) })
	{
		Box(EZoneMat::Fabric, TentPos + FVector(0.f, 0.f, 90.f), FVector(500.f, 350.f, 180.f));
		Box(EZoneMat::Fabric, TentPos + FVector(0.f, 0.f, 210.f), FVector(500.f, 200.f, 60.f));
	}

	// Mirador avec projecteur braqué sur la rue
	const FVector TowerPos(CX + 250.f, 780.f, 15.f);
	for (int32 SX : { 1, -1 })
	{
		for (int32 SY : { 1, -1 })
		{
			Cyl(EZoneMat::Wood, TowerPos + FVector(SX * 90.f, SY * 90.f, 260.f), 20.f, 520.f);
		}
	}
	Box(EZoneMat::Wood, TowerPos + FVector(0.f, 0.f, 520.f), FVector(240.f, 240.f, 15.f));
	Box(EZoneMat::Wood, TowerPos + FVector(-115.f, 0.f, 570.f), FVector(10.f, 240.f, 90.f));
	Box(EZoneMat::Wood, TowerPos + FVector(0.f, 0.f, 680.f), FVector(260.f, 260.f, 12.f));
	Box(EZoneMat::Rust, TowerPos + FVector(-80.f, -60.f, 600.f), FVector(40.f, 50.f, 40.f));

	const FVector LampPos = TowerPos + FVector(-110.f, -60.f, 600.f);
	const FVector LampTarget(CX - 1400.f, 0.f, 0.f);
	Floodlight->SetRelativeLocation(LampPos);
	Floodlight->SetRelativeRotation((LampTarget - LampPos).Rotation());

	// Feux de baril autour du camp
	BuildFireBarrel(FVector(CX + 450.f, 420.f, 0.f));
	BuildFireBarrel(FVector(CX + 1100.f, -400.f, 0.f));

	// Hélisurface marquée au sol (point d'extraction)
	const FVector Pad = ExtractionLocal;
	Box(EZoneMat::Concrete, FVector(Pad.X, Pad.Y, -23.5f), FVector(900.f, 900.f, 50.f));
	Box(EZoneMat::Line, FVector(Pad.X, Pad.Y - 130.f, 2.f), FVector(420.f, 50.f, 1.f));
	Box(EZoneMat::Line, FVector(Pad.X, Pad.Y + 130.f, 2.f), FVector(420.f, 50.f, 1.f));
	Box(EZoneMat::Line, FVector(Pad.X, Pad.Y, 2.f), FVector(50.f, 210.f, 1.f));
}

void AZoneBuilder::BuildDebris(FRandomStream& R)
{
	using namespace ZoneLayout;
	const EZoneMat Mats[] = { EZoneMat::Concrete, EZoneMat::ConcreteDark, EZoneMat::Rust, EZoneMat::Wood, EZoneMat::Brick, EZoneMat::Fabric };

	for (int32 i = 0; i < NumDebris; ++i)
	{
		const FVector Pos2D(R.FRandRange(StartX + 600.f, StreetLength - 1700.f), R.FRandRange(-SidewalkOuter + 40.f, SidewalkOuter - 40.f), 0.f);
		if (FVector::Dist2D(Pos2D, PlayerStartLocal) < 300.f)
		{
			continue;
		}
		const FVector Size(R.FRandRange(15.f, 75.f), R.FRandRange(15.f, 75.f), R.FRandRange(8.f, 40.f));
		const float Ground = FMath::Abs(Pos2D.Y) > RoadHalfWidth ? 15.f : 1.f;
		Box(Mats[R.RandRange(0, 5)], FVector(Pos2D.X, Pos2D.Y, Ground + Size.Z * 0.35f), Size,
			FRotator(R.FRandRange(-12.f, 12.f), R.FRandRange(0.f, 360.f), R.FRandRange(-12.f, 12.f)));
	}

	// Sacs poubelle entassés au bord des trottoirs
	for (int32 i = 0; i < 12; ++i)
	{
		const int32 Side = R.FRand() < 0.5f ? 1 : -1;
		const FVector Center(R.FRandRange(500.f, StreetLength - 2500.f), Side * (SidewalkOuter - 60.f), 15.f);
		const int32 Count = R.RandRange(3, 7);
		for (int32 b = 0; b < Count; ++b)
		{
			const FVector Offset(R.FRandRange(-80.f, 80.f), R.FRandRange(-40.f, 20.f) * Side, 0.f);
			const float S = R.FRandRange(45.f, 65.f);
			Box(EZoneMat::Burnt, Center + Offset + FVector(0.f, 0.f, S * 0.4f), FVector(S, S * 0.9f, S * 0.8f), FRotator(R.FRandRange(-15.f, 15.f), R.FRandRange(0.f, 90.f), R.FRandRange(-15.f, 15.f)));
		}
	}
}

// =====================================================================
//  Objets de gameplay
// =====================================================================

void AZoneBuilder::SpawnGameplayActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FTransform& ActorT = GetActorTransform();
	FRandomStream R(Seed + 77);

	// Extraction (sauf si tu en as déjà placé une à la main)
	bool bHasExtraction = false;
	for (TActorIterator<AExtractionZone> It(World); It; ++It)
	{
		bHasExtraction = true;
		break;
	}
	if (!bHasExtraction)
	{
		World->SpawnActor<AExtractionZone>(AExtractionZone::StaticClass(), ActorT.TransformPosition(ExtractionLocal), ActorT.Rotator());
	}

	// Objectifs (sauf si tu en as déjà placé à la main)
	bool bHasObjectives = false;
	for (TActorIterator<AItemPickup> It(World); It; ++It)
	{
		if (It->bIsObjective)
		{
			bHasObjectives = true;
			break;
		}
	}

	TArray<FVector> Spots = InteriorSpots;
	Spots.Sort([](const FVector& A, const FVector& B) { return A.X < B.X; });

	auto TakeSpot = [&Spots](float Fraction, FVector& Out) -> bool
	{
		if (Spots.Num() == 0)
		{
			return false;
		}
		const int32 Index = FMath::Clamp(FMath::FloorToInt(Fraction * Spots.Num()), 0, Spots.Num() - 1);
		Out = Spots[Index];
		Spots.RemoveAt(Index);
		return true;
	};

	auto SpawnItem = [&](UClass* Class, const FVector& LocalPos, TFunctionRef<void(AItemPickup*)> Setup)
	{
		const FTransform SpawnT(FRotator(0.f, R.FRandRange(0.f, 360.f), 0.f), ActorT.TransformPosition(LocalPos));
		AItemPickup* Item = World->SpawnActorDeferred<AItemPickup>(Class, SpawnT, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (Item)
		{
			Setup(Item);
			Item->FinishSpawning(SpawnT);
		}
	};

	if (!bHasObjectives)
	{
		struct FObjectiveDef
		{
			const TCHAR* Id;
			FText Name;
			float Weight;
			FLinearColor Color;
			FVector Scale;
			float Fraction;
		};
		const FObjectiveDef Defs[] =
		{
			{ TEXT("Fuel"),    NSLOCTEXT("LastSignal", "ObjFuel", "Jerrican d'essence"),  9.f, FLinearColor(0.35f, 0.04f, 0.02f), FVector(0.35f, 0.17f, 0.45f), 0.25f },
			{ TEXT("Battery"), NSLOCTEXT("LastSignal", "ObjBattery", "Batterie de radio"), 1.5f, FLinearColor(0.25f, 0.22f, 0.05f), FVector(0.2f, 0.12f, 0.1f), 0.55f },
			{ TEXT("Medkit"),  NSLOCTEXT("LastSignal", "ObjMedkit", "Trousse de secours"), 2.f, FLinearColor(0.5f, 0.5f, 0.48f), FVector(0.35f, 0.25f, 0.12f), 0.85f },
		};

		for (const FObjectiveDef& Def : Defs)
		{
			FVector Spot;
			if (!TakeSpot(Def.Fraction, Spot))
			{
				break;
			}
			SpawnItem(AItemPickup::StaticClass(), Spot, [&Def](AItemPickup* Item)
			{
				Item->ItemId = FName(Def.Id);
				Item->DisplayName = Def.Name;
				Item->Weight = Def.Weight;
				Item->Color = Def.Color;
				Item->MeshScale = Def.Scale;
				Item->bIsObjective = true;
				Item->bGlint = true;
			});
		}
	}

	// Munitions : rares, dispersées dans les bâtiments restants
	for (int32 i = 0; i < NumAmmoPickups; ++i)
	{
		FVector Spot;
		if (!TakeSpot(R.FRand(), Spot))
		{
			break;
		}
		SpawnItem(AAmmoPickup::StaticClass(), Spot, [&R](AItemPickup* Item)
		{
			if (AAmmoPickup* Ammo = Cast<AAmmoPickup>(Item))
			{
				Ammo->AmmoAmount = R.RandRange(4, 8);
			}
		});
	}
}

// =====================================================================
//  Lumières animées
// =====================================================================

int32 AZoneBuilder::AddLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius, ELightMode Mode, bool bShadows)
{
	if (!PointLights.IsValidIndex(NextLight) || !PointLights[NextLight])
	{
		return INDEX_NONE;
	}
	UPointLightComponent* Light = PointLights[NextLight];
	Light->SetRelativeLocation(Location);
	Light->SetLightColor(Color);
	Light->SetIntensity(Intensity);
	Light->SetAttenuationRadius(Radius);
	Light->SetCastShadows(bShadows);
	Light->SetVolumetricScatteringIntensity(Mode == ELightMode::Fire ? 2.f : 1.f);
	Light->SetVisibility(true);

	FLightAnim Anim;
	Anim.LightIndex = NextLight;
	Anim.BaseIntensity = Intensity;
	Anim.Mode = Mode;
	Anim.Seed = NextLight * 17.31f;
	LightAnims.Add(Anim);
	return NextLight++;
}

void AZoneBuilder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float T = GetWorld()->GetTimeSeconds();
	for (const FLightAnim& Anim : LightAnims)
	{
		UPointLightComponent* Light = PointLights.IsValidIndex(Anim.LightIndex) ? PointLights[Anim.LightIndex].Get() : nullptr;
		if (!Light)
		{
			continue;
		}

		float K = 1.f;
		switch (Anim.Mode)
		{
		case ELightMode::Fire:
			K = 0.8f + 0.2f * FMath::PerlinNoise1D(T * 5.f + Anim.Seed) + 0.08f * FMath::Sin(T * 23.f + Anim.Seed);
			break;
		case ELightMode::Broken:
			// La plupart du temps allumée, puis par moments elle grésille
			if (FMath::PerlinNoise1D(T * 0.7f + Anim.Seed) > 0.2f)
			{
				K = FMath::FRand() < 0.4f ? 0.05f : 1.f;
			}
			break;
		default:
			break;
		}
		Light->SetIntensity(Anim.BaseIntensity * K);
	}
}

// =====================================================================
//  Outils de construction
// =====================================================================

void AZoneBuilder::Box(EZoneMat M, const FVector& Center, const FVector& Size, const FRotator& Rot)
{
	const int32 I = static_cast<int32>(M);
	if (CubeISMs.IsValidIndex(I) && CubeISMs[I])
	{
		CubeISMs[I]->AddInstance(FTransform(Rot, Center, Size / 100.f), false);
	}
}

void AZoneBuilder::BoxT(EZoneMat M, const FTransform& Parent, const FVector& LocalCenter, const FVector& Size, const FRotator& LocalRot)
{
	const int32 I = static_cast<int32>(M);
	if (CubeISMs.IsValidIndex(I) && CubeISMs[I])
	{
		CubeISMs[I]->AddInstance(FTransform(LocalRot, LocalCenter, Size / 100.f) * Parent, false);
	}
}

void AZoneBuilder::Cyl(EZoneMat M, const FVector& Center, float Diameter, float Height, const FRotator& Rot)
{
	const int32 I = static_cast<int32>(M);
	if (CylinderISMs.IsValidIndex(I) && CylinderISMs[I])
	{
		CylinderISMs[I]->AddInstance(FTransform(Rot, Center, FVector(Diameter / 100.f, Diameter / 100.f, Height / 100.f)), false);
	}
}

void AZoneBuilder::CylT(EZoneMat M, const FTransform& Parent, const FVector& LocalCenter, float Diameter, float Height, const FRotator& LocalRot)
{
	const int32 I = static_cast<int32>(M);
	if (CylinderISMs.IsValidIndex(I) && CylinderISMs[I])
	{
		CylinderISMs[I]->AddInstance(FTransform(LocalRot, LocalCenter, FVector(Diameter / 100.f, Diameter / 100.f, Height / 100.f)) * Parent, false);
	}
}

void AZoneBuilder::WallSegment(EZoneMat M, const FVector& Start, bool bAlongX, float A, float B, float Z0, float Z1, float Thickness)
{
	if (B - A < 1.f || Z1 - Z0 < 1.f)
	{
		return;
	}
	const float Mid = (A + B) * 0.5f;
	FVector Center = Start + (bAlongX ? FVector(Mid, 0.f, 0.f) : FVector(0.f, Mid, 0.f));
	Center.Z = Start.Z + (Z0 + Z1) * 0.5f;
	const FVector Size = bAlongX ? FVector(B - A, Thickness, Z1 - Z0) : FVector(Thickness, B - A, Z1 - Z0);
	Box(M, Center, Size);
}

void AZoneBuilder::Wall(EZoneMat M, const FVector& Start, bool bAlongX, float Length, float Height, float Thickness, TArray<FWallOpening> Openings)
{
	Openings.Sort([](const FWallOpening& A, const FWallOpening& B) { return A.Offset < B.Offset; });

	float Cursor = 0.f;
	for (const FWallOpening& Op : Openings)
	{
		const float A = FMath::Clamp(Op.Offset, Cursor, Length);
		const float B = FMath::Clamp(Op.Offset + Op.Width, A, Length);
		WallSegment(M, Start, bAlongX, Cursor, A, 0.f, Height, Thickness);           // plein avant l'ouverture
		WallSegment(M, Start, bAlongX, A, B, 0.f, FMath::Min(Op.Bottom, Height), Thickness); // allège
		WallSegment(M, Start, bAlongX, A, B, FMath::Min(Op.Top, Height), Height, Thickness); // linteau
		Cursor = B;
	}
	WallSegment(M, Start, bAlongX, Cursor, Length, 0.f, Height, Thickness);
}

void AZoneBuilder::JaggedWall(FRandomStream& R, EZoneMat M, const FVector& Start, bool bAlongX, float Length, float MaxHeight, float Thickness)
{
	// Mur effondré : morceaux de hauteurs irrégulières
	float A = 0.f;
	while (A < Length)
	{
		const float Seg = FMath::Min(R.FRandRange(60.f, 170.f), Length - A);
		const float Height = R.FRand() < 0.25f ? 0.f : R.FRandRange(MaxHeight * 0.15f, MaxHeight);
		WallSegment(M, Start, bAlongX, A, A + Seg, 0.f, Height, Thickness);
		A += Seg;
	}
}
