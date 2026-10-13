#include "World/ExtractionZone.h"

#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/ZoneGameMode.h"
#include "Player/BodycamCharacter.h"

AExtractionZone::AExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(350.f, 350.f, 200.f));
	Trigger->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	FlareMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flare"));
	FlareMesh->SetupAttachment(Trigger);
	if (CylinderMesh.Succeeded())
	{
		FlareMesh->SetStaticMesh(CylinderMesh.Object);
	}
	FlareMesh->SetRelativeLocation(FVector(0.f, 0.f, -195.f));
	FlareMesh->SetRelativeRotation(FRotator(0.f, 0.f, 90.f)); // couché au sol
	FlareMesh->SetRelativeScale3D(FVector(0.05f, 0.05f, 0.25f));
	FlareMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FlareLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlareLight"));
	FlareLight->SetupAttachment(Trigger);
	FlareLight->SetRelativeLocation(FVector(0.f, 0.f, -150.f));
	FlareLight->IntensityUnits = ELightUnits::Candelas;
	FlareLight->Intensity = FlareIntensity;
	FlareLight->AttenuationRadius = 2200.f;
	FlareLight->LightColor = FColor(255, 40, 25);
	FlareLight->VolumetricScatteringIntensity = 3.f; // halo rouge dans le brouillard
	FlareLight->SetMobility(EComponentMobility::Movable);
}

void AExtractionZone::BeginPlay()
{
	Super::BeginPlay();

	// L'acteur est posé au sol : on remonte la zone pour qu'elle couvre le joueur
	Trigger->AddLocalOffset(FVector(0.f, 0.f, 200.f));
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AExtractionZone::OnZoneBeginOverlap);

	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.8f, 0.05f, 0.02f));
		FlareMesh->SetMaterial(0, MID);
	}
}

void AExtractionZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Un fumigène crépite : intensité irrégulière
	const float T = GetWorld()->GetTimeSeconds();
	const float Noise = FMath::PerlinNoise1D(T * 7.f) * 0.25f + FMath::Sin(T * 31.f) * 0.08f;
	FlareLight->SetIntensity(FlareIntensity * (0.85f + Noise));

	// Si le joueur attend dans la zone, on revérifie régulièrement
	CheckTimer -= DeltaSeconds;
	if (CheckTimer <= 0.f)
	{
		CheckTimer = 0.5f;
		CheckOverlaps();
	}
}

void AExtractionZone::OnZoneBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ABodycamCharacter* Character = Cast<ABodycamCharacter>(OtherActor))
	{
		if (AZoneGameMode* GM = GetWorld()->GetAuthGameMode<AZoneGameMode>())
		{
			GM->TryExtract(Character);
		}
	}
}

void AExtractionZone::CheckOverlaps()
{
	AZoneGameMode* GM = GetWorld()->GetAuthGameMode<AZoneGameMode>();
	if (!GM || !GM->AreAllObjectivesCollected())
	{
		return;
	}
	TArray<AActor*> Overlapping;
	Trigger->GetOverlappingActors(Overlapping, ABodycamCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		GM->TryExtract(Cast<ABodycamCharacter>(Actor));
	}
}
