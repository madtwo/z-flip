#include "GSFramework.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

#include "GSGravityManager.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
#include "GSRollingBallPawn.h"
#include "GSWorldState.h"

// ---------------------------------------------------------------------------------
// Game mode
// ---------------------------------------------------------------------------------

AGSGravityGameMode::AGSGravityGameMode()
{
	DefaultPawnClass = AGSRollingBallPawn::StaticClass();
	HUDClass = AGSGravityHUD::StaticClass();

	GravityManagerClass = AGSGravityManager::StaticClass();
	WorldStateManagerClass = AGSWorldStateManager::StaticClass();
}

void AGSGravityGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSpawnCoreManagers)
	{
		EnsureCoreManagers();
	}
}

void AGSGravityGameMode::EnsureCoreManagers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!AGSGravityManager::FindGravityManager(this) && GravityManagerClass)
	{
		AGSGravityManager* Manager = World->SpawnActor<AGSGravityManager>(GravityManagerClass, FTransform::Identity);
		Manager->SetActorLabel(TEXT("GravityShift_Manager"));
		UE_LOG(LogTemp, Log, TEXT("[GravityShift] game mode spawned gravity manager"));
	}

	if (!AGSWorldStateManager::FindWorldStateManager(this) && WorldStateManagerClass)
	{
		AGSWorldStateManager* StateManager = World->SpawnActor<AGSWorldStateManager>(WorldStateManagerClass, FTransform::Identity);
		StateManager->SetActorLabel(TEXT("GravityShift_WorldState"));
		UE_LOG(LogTemp, Log, TEXT("[GravityShift] game mode spawned world state manager"));
	}
}

void AGSGravityGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	AGSRollingBallPawn* Ball = NewPlayer ? Cast<AGSRollingBallPawn>(NewPlayer->GetPawn()) : nullptr;
	if (Ball)
	{
		if (DefaultBallProfile)
		{
			Ball->ApplyBallProfile(DefaultBallProfile);
		}
		Ball->RefreshSystemReferences();
	}
}

// ---------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------

AGSGravityHUD::AGSGravityHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGSGravityHUD::SetHUDVisible(bool bVisible)
{
	bHUDVisible = bVisible;
}

void AGSGravityHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bHUDVisible || !Canvas || !GEngine)
	{
		return;
	}

	AGSRollingBallPawn* Ball = PlayerOwner ? Cast<AGSRollingBallPawn>(PlayerOwner->GetPawn()) : nullptr;
	if (!Ball)
	{
		return;
	}

	TArray<FString> Lines;

	if (bShowGravityStatus)
	{
		AGSGravityManager* Manager = Ball->GravityManager;
		const EGSGravityPolarity Polarity = Ball->GetCurrentGravityPolarity();
		Lines.Add(FString::Printf(TEXT("Gravity: %s   (rev %d)"),
			Polarity == EGSGravityPolarity::POSITIVE_Z ? TEXT("+Z  CEILING") : TEXT("-Z  FLOOR"),
			Manager ? Manager->GravityRevision : 0));
		Lines.Add(FString::Printf(TEXT("Camera up: (%.2f, %.2f, %.2f)"),
			Ball->GetCameraUpVector().X, Ball->GetCameraUpVector().Y, Ball->GetCameraUpVector().Z));
	}

	if (bShowFallStatus && Ball->LandingResponse)
	{
		UGSLandingResponseComponent* Landing = Ball->LandingResponse;
		Lines.Add(FString::Printf(TEXT("Supported: %s   fall %.0f cm/s   dist %.0f cm   air %.2f s"),
			Landing->IsSupported() ? TEXT("yes") : TEXT("no"),
			Landing->GetCurrentFallSpeedCm(),
			Landing->GetCurrentFallDistanceCm(),
			Landing->GetAirborneSeconds()));
	}

	if (bShowCollectibleStatus && Ball->WorldStateManager)
	{
		Lines.Add(FString::Printf(TEXT("Collected: %.0f / %.0f"),
			Ball->WorldStateManager->CollectedValue,
			Ball->WorldStateManager->GetRequiredGoalValue()));
	}

	if (bShowInteractionPrompt)
	{
		const FText Prompt = Ball->GetCurrentInteractionText();
		if (!Prompt.IsEmpty())
		{
			Lines.Add(Prompt.ToString());
		}
	}

	if (bShowControls)
	{
		Lines.Add(TEXT("WASD roll  |  mouse look  |  G flip gravity  |  E interact  |  R reset"));
	}

	float Y = StartPosition.Y;
	Canvas->SetDrawColor(TextColor.ToFColor(true));
	for (const FString& Line : Lines)
	{
		Canvas->DrawText(GEngine->GetMediumFont(), Line, StartPosition.X, Y, 1.0f, 1.0f);
		Y += LineHeight;
	}
}
