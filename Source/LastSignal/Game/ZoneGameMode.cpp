#include "Game/ZoneGameMode.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"

#include "Items/ItemPickup.h"
#include "Player/BodycamCharacter.h"
#include "Player/BodycamPlayerController.h"
#include "UI/BodycamHUD.h"
#include "World/ZoneBuilder.h"

AZoneGameMode::AZoneGameMode()
{
	DefaultPawnClass = ABodycamCharacter::StaticClass();
	PlayerControllerClass = ABodycamPlayerController::StaticClass();
	HUDClass = ABodycamHUD::StaticClass();
}

void AZoneGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Utilise BP_Player s'il a été créé dans l'éditeur
	if (!PlayerBlueprintPath.IsEmpty() && FPackageName::DoesPackageExist(PlayerBlueprintPath))
	{
		const FString ClassPath = PlayerBlueprintPath + TEXT(".") + FPackageName::GetShortName(PlayerBlueprintPath) + TEXT("_C");
		if (UClass* PlayerClass = LoadClass<APawn>(nullptr, *ClassPath))
		{
			DefaultPawnClass = PlayerClass;
		}
	}
}

void AZoneGameMode::StartPlay()
{
	EnsureZoneBuilder();
	Super::StartPlay();
}

AZoneBuilder* AZoneGameMode::EnsureZoneBuilder()
{
	if (CachedBuilder)
	{
		return CachedBuilder;
	}
	UWorld* World = GetWorld();
	TActorIterator<AZoneBuilder> ExistingBuilder(World);
	if (ExistingBuilder)
	{
		CachedBuilder = *ExistingBuilder;
		return CachedBuilder;
	}
	if (!bAutoSpawnZoneBuilder)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	CachedBuilder = World->SpawnActor<AZoneBuilder>(AZoneBuilder::StaticClass(), FTransform::Identity, Params);
	return CachedBuilder;
}

AActor* AZoneGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	AActor* Start = Super::ChoosePlayerStart_Implementation(Player);
	if (Start && Start->IsA<APlayerStart>())
	{
		return Start;
	}

	// Pas de PlayerStart dans la map : on utilise le point de départ prévu par le ZoneBuilder
	if (!RuntimePlayerStart)
	{
		AZoneBuilder* Builder = EnsureZoneBuilder();
		const FTransform StartTransform = Builder ? Builder->GetPlayerStartTransform() : FTransform(FVector(0.f, 0.f, 120.f));
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RuntimePlayerStart = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), StartTransform, Params);
	}
	return RuntimePlayerStart ? RuntimePlayerStart.Get() : Start;
}

void AZoneGameMode::RegisterObjective(AItemPickup* Pickup)
{
	if (!Pickup)
	{
		return;
	}
	FZoneObjective Objective;
	Objective.ItemId = Pickup->ItemId;
	Objective.DisplayName = Pickup->DisplayName;
	Objectives.Add(Objective);
}

void AZoneGameMode::OnObjectiveCollected(AItemPickup* Pickup, ABodycamCharacter* Collector)
{
	if (!Pickup)
	{
		return;
	}
	for (FZoneObjective& Objective : Objectives)
	{
		if (!Objective.bCollected && Objective.ItemId == Pickup->ItemId)
		{
			Objective.bCollected = true;
			break;
		}
	}

	if (Collector && AreAllObjectivesCollected())
	{
		Collector->ShowMessage(NSLOCTEXT("LastSignal", "AllCollected",
			"J'ai tout. Direction le point d'extraction, au bout de la rue."), 5.f);
	}
}

int32 AZoneGameMode::GetCollectedCount() const
{
	int32 Count = 0;
	for (const FZoneObjective& Objective : Objectives)
	{
		Count += Objective.bCollected ? 1 : 0;
	}
	return Count;
}

bool AZoneGameMode::AreAllObjectivesCollected() const
{
	return GetCollectedCount() >= Objectives.Num();
}

void AZoneGameMode::TryExtract(ABodycamCharacter* Character)
{
	if (!Character || ZoneState != EZoneState::Playing || Character->IsDead())
	{
		return;
	}

	if (!AreAllObjectivesCollected())
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastExtractDeniedTime > 4.f)
		{
			LastExtractDeniedTime = Now;
			const int32 Missing = Objectives.Num() - GetCollectedCount();
			Character->ShowMessage(FText::Format(NSLOCTEXT("LastSignal", "MissingItems",
				"Radio : \"Négatif, il nous manque encore {0} élément(s). On ne décolle pas sans.\""),
				FText::AsNumber(Missing)), 4.f);
		}
		return;
	}

	EndZone(EZoneState::Extracted, Character);
}

void AZoneGameMode::OnPlayerDied(ABodycamCharacter* Character)
{
	if (ZoneState == EZoneState::Playing)
	{
		EndZone(EZoneState::Dead, Character);
	}
}

void AZoneGameMode::EndZone(EZoneState NewState, ABodycamCharacter* Character)
{
	ZoneState = NewState;
	EndTime = GetWorld()->GetTimeSeconds();
	if (Character)
	{
		Character->SetGameplayInputEnabled(false);
	}
	GetWorldTimerManager().SetTimer(RestartTimer, this, &AZoneGameMode::RestartZone, RestartDelay, false);
}

float AZoneGameMode::GetTimeSinceEnd() const
{
	return ZoneState == EZoneState::Playing ? 0.f : GetWorld()->GetTimeSeconds() - EndTime;
}

void AZoneGameMode::RestartZone()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)));
}
