#include "Items/ItemPickup.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "Game/ZoneGameMode.h"
#include "Items/InventoryComponent.h"
#include "Player/BodycamCharacter.h"

AItemPickup::AItemPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	DisplayName = NSLOCTEXT("LastSignal", "DefaultItem", "Objet");

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	// Visible par le "regard" du joueur, mais on ne bute pas dedans
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	GlintLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glint"));
	GlintLight->SetupAttachment(Root);
	GlintLight->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
	GlintLight->IntensityUnits = ELightUnits::Candelas;
	GlintLight->Intensity = 1.5f;
	GlintLight->AttenuationRadius = 90.f;
	GlintLight->LightColor = FColor(255, 235, 200);
	GlintLight->SetCastShadows(false);
	GlintLight->SetVisibility(false);
}

void AItemPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyVisuals();
}

void AItemPickup::BeginPlay()
{
	Super::BeginPlay();
	ApplyVisuals();

	if (bIsObjective)
	{
		if (AZoneGameMode* GM = GetWorld()->GetAuthGameMode<AZoneGameMode>())
		{
			GM->RegisterObjective(this);
		}
	}
}

void AItemPickup::ApplyVisuals()
{
	// Le bloc est posé au sol : son bas est à l'origine de l'acteur
	Mesh->SetRelativeScale3D(MeshScale);
	Mesh->SetRelativeLocation(FVector(0.f, 0.f, MeshScale.Z * 50.f));
	GlintLight->SetVisibility(bGlint);

	if (UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMat, this);
		MID->SetFlags(RF_Transient);
		MID->SetVectorParameterValue(TEXT("Color"), Color);
		Mesh->SetMaterial(0, MID);
	}
}

FText AItemPickup::GetInteractionText_Implementation() const
{
	return FText::Format(NSLOCTEXT("LastSignal", "PickupPrompt", "Ramasser : {0}"), DisplayName);
}

void AItemPickup::Interact_Implementation(ABodycamCharacter* User)
{
	if (!User || IsActorBeingDestroyed())
	{
		return;
	}

	FInventoryItem Item;
	Item.ItemId = ItemId;
	Item.DisplayName = DisplayName;
	Item.Weight = Weight;
	Item.bIsObjective = bIsObjective;
	User->GetInventory()->AddItem(Item);

	FNumberFormattingOptions Fmt;
	Fmt.MinimumFractionalDigits = 1;
	Fmt.MaximumFractionalDigits = 1;
	User->ShowMessage(FText::Format(NSLOCTEXT("LastSignal", "PickedUp", "{0} ajouté au sac (+{1} kg)"),
		DisplayName, FText::AsNumber(Weight, &Fmt)));

	if (bIsObjective)
	{
		if (AZoneGameMode* GM = GetWorld()->GetAuthGameMode<AZoneGameMode>())
		{
			GM->OnObjectiveCollected(this, User);
		}
	}

	Destroy();
}
