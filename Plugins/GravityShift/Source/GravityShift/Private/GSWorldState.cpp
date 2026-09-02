#include "GSWorldState.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

#include "GSBreakableComponent.h"
#include "GSGravityManager.h"
#include "GSProfiles.h"
#include "GSResettableComponent.h"

// ---------------------------------------------------------------------------------
// World state manager
// ---------------------------------------------------------------------------------

AGSWorldStateManager::AGSWorldStateManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

AGSWorldStateManager* AGSWorldStateManager::FindWorldStateManager(UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGSWorldStateManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void AGSWorldStateManager::RegisterPlayer(APawn* NewPlayer)
{
	RegisteredPlayer = NewPlayer;
}

APawn* AGSWorldStateManager::GetRegisteredPlayer() const
{
	return RegisteredPlayer;
}

void AGSWorldStateManager::SetCheckpoint(FTransform NewCheckpointTransform)
{
	ActiveCheckpointTransform = NewCheckpointTransform;
	HasActiveCheckpoint = true;
}

void AGSWorldStateManager::CaptureWorldCheckpointState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (UGSResettableComponent* Resettable = Actor->FindComponentByClass<UGSResettableComponent>())
		{
			Resettable->CaptureCheckpointState();
		}
	}
}

void AGSWorldStateManager::ResetWorld()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (AGSGravityManager* Manager = AGSGravityManager::FindGravityManager(this))
	{
		Manager->ResetGravity(true);
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}

		if (UGSBreakableComponent* Breakable = Actor->FindComponentByClass<UGSBreakableComponent>())
		{
			Breakable->Repair();
		}

		if (UGSResettableComponent* Resettable = Actor->FindComponentByClass<UGSResettableComponent>())
		{
			if (bUseCapturedCheckpointState)
			{
				Resettable->RestoreCheckpointState();
			}
			else
			{
				Resettable->RestoreInitialState();
			}
		}

		AGSCollectible* Collectible = Cast<AGSCollectible>(Actor);
		if (Collectible)
		{
			Collectible->RestoreCheckpointState();
		}
	}

	if (RegisteredPlayer && HasActiveCheckpoint)
	{
		RegisteredPlayer->SetActorTransform(ActiveCheckpointTransform, false, nullptr, ETeleportType::TeleportPhysics);
		if (UGSResettableComponent* Resettable = RegisteredPlayer->FindComponentByClass<UGSResettableComponent>())
		{
			Resettable->TeleportAndReset(ActiveCheckpointTransform);
		}
	}

	RecalculateCollectibleCounts();
	UE_LOG(LogTemp, Log, TEXT("[GravityShift] world reset"));
}

void AGSWorldStateManager::RegisterCollectible(AGSCollectible* Collectible)
{
	if (Collectible && !RegisteredCollectibles.Contains(Collectible))
	{
		RegisteredCollectibles.Add(Collectible);
		RecalculateCollectibleCounts();
	}
}

void AGSWorldStateManager::NotifyCollectibleStateChanged(AGSCollectible* Collectible, bool bCollected)
{
	RecalculateCollectibleCounts();
}

void AGSWorldStateManager::RecalculateCollectibleCounts()
{
	TotalCollectibleValue = 0.0f;
	CollectedValue = 0.0f;
	TotalRequiredCollectibleValue = 0.0f;

	for (const TObjectPtr<AGSCollectible>& Item : RegisteredCollectibles)
	{
		if (!Item)
		{
			continue;
		}

		TotalCollectibleValue += Item->Value;
		if (Item->bRequiredForGoal)
		{
			TotalRequiredCollectibleValue += Item->Value;
		}
		if (Item->bCollected)
		{
			CollectedValue += Item->Value;
		}
	}
}

float AGSWorldStateManager::GetRequiredGoalValue() const
{
	return RequiredValueOverride >= 0.0f ? RequiredValueOverride : TotalRequiredCollectibleValue;
}

bool AGSWorldStateManager::CanCompleteGoal() const
{
	return CollectedValue + KINDA_SMALL_NUMBER >= GetRequiredGoalValue();
}

// ---------------------------------------------------------------------------------
// Checkpoint
// ---------------------------------------------------------------------------------

AGSCheckpoint::AGSCheckpoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(200.0, 200.0, 200.0));
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);
}

void AGSCheckpoint::BeginPlay()
{
	Super::BeginPlay();

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGSCheckpoint::HandleBeginOverlap);
}

void AGSCheckpoint::ActivateCheckpoint(AActor* Activator)
{
	if (bOneShot && bActivated)
	{
		return;
	}

	bActivated = true;

	FTransform Respawn = GetRespawnTransform();
	if (WorldStateManager)
	{
		WorldStateManager->SetCheckpoint(Respawn);
		if (bCaptureWorldState)
		{
			WorldStateManager->CaptureWorldCheckpointState();
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] checkpoint %s activated at %s"),
		*GetNameSafe(this), *Respawn.GetLocation().ToString());
}

FTransform AGSCheckpoint::GetRespawnTransform() const
{
	FTransform Base = GetActorTransform();
	Base.SetLocation(Base.GetLocation() + RespawnOffset.GetLocation());
	return Base;
}

void AGSCheckpoint::ResetCheckpoint()
{
	bActivated = false;
}

void AGSCheckpoint::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(APawn::StaticClass()))
	{
		ActivateCheckpoint(OtherActor);
	}
}

// ---------------------------------------------------------------------------------
// Kill volume
// ---------------------------------------------------------------------------------

AGSKillVolume::AGSKillVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(VolumeExtent);
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);
	SetRootComponent(Volume);
}

void AGSKillVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetVolumeExtent(VolumeExtent);
}

void AGSKillVolume::BeginPlay()
{
	Super::BeginPlay();

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
	}

	Volume->OnComponentBeginOverlap.AddDynamic(this, &AGSKillVolume::HandleBeginOverlap);
}

void AGSKillVolume::SetVolumeExtent(FVector NewExtent)
{
	VolumeExtent = NewExtent;
	if (Volume)
	{
		Volume->SetBoxExtent(NewExtent);
	}
}

void AGSKillVolume::TriggerReset()
{
	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastTriggerTime < TriggerCooldownSeconds)
	{
		return;
	}
	LastTriggerTime = Now;

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
	}

	if (WorldStateManager)
	{
		WorldStateManager->ResetWorld();
	}
}

void AGSKillVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(APawn::StaticClass()))
	{
		TriggerReset();
	}
}

// ---------------------------------------------------------------------------------
// Collectible
// ---------------------------------------------------------------------------------

AGSCollectible::AGSCollectible()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Mesh->SetGenerateOverlapEvents(true);
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		Mesh->SetStaticMesh(SphereAsset.Object);
		Mesh->SetRelativeScale3D(FVector(0.4));
	}
}

void AGSCollectible::BeginPlay()
{
	Super::BeginPlay();

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
	}

	if (WorldStateManager)
	{
		WorldStateManager->RegisterCollectible(this);
	}

	bInitialCollected = bCollected;
	CaptureCheckpointState();

	Mesh->OnComponentBeginOverlap.AddDynamic(this, &AGSCollectible::HandleBeginOverlap);
}

void AGSCollectible::ApplyCollectibleProfile(UGSCollectibleProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	CollectibleProfile = NewProfile;
	Value = NewProfile->Value;
	bPersistThroughReset = NewProfile->bPersistThroughReset;
	bRequiredForGoal = NewProfile->bRequiredForGoal;
}

bool AGSCollectible::Collect(APawn* Collector)
{
	if (bCollected)
	{
		return false;
	}

	bCollected = true;
	Mesh->SetHiddenInGame(true);

	if (WorldStateManager)
	{
		WorldStateManager->NotifyCollectibleStateChanged(this, true);
	}

	return true;
}

void AGSCollectible::SetCollectedState(bool bNewCollected)
{
	bCollected = bNewCollected;
	Mesh->SetHiddenInGame(bNewCollected);

	if (WorldStateManager)
	{
		WorldStateManager->NotifyCollectibleStateChanged(this, bNewCollected);
	}
}

void AGSCollectible::CaptureCheckpointState()
{
	bCheckpointCollected = bCollected;
}

void AGSCollectible::RestoreCheckpointState()
{
	if (bPersistThroughReset)
	{
		SetCollectedState(bCheckpointCollected);
	}
	else
	{
		SetCollectedState(bInitialCollected);
	}
}

void AGSCollectible::RestoreInitialState()
{
	SetCollectedState(bInitialCollected);
}

void AGSCollectible::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (Pawn)
	{
		Collect(Pawn);
	}
}

// ---------------------------------------------------------------------------------
// Finish goal
// ---------------------------------------------------------------------------------

AGSFinishGoal::AGSFinishGoal()
{
	PrimaryActorTick.bCanEverTick = false;

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetBoxExtent(FVector(200.0, 200.0, 200.0));
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	SetRootComponent(Trigger);
}

void AGSFinishGoal::BeginPlay()
{
	Super::BeginPlay();

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
	}

	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGSFinishGoal::HandleBeginOverlap);
}

bool AGSFinishGoal::CanComplete() const
{
	if (!bRequireCollectibles)
	{
		return true;
	}

	return WorldStateManager ? WorldStateManager->CanCompleteGoal() : true;
}

bool AGSFinishGoal::TryComplete()
{
	if (!CanComplete())
	{
		return false;
	}

	bCompleted = true;
	UE_LOG(LogTemp, Log, TEXT("[GravityShift] GOAL COMPLETE"));
	return true;
}

void AGSFinishGoal::ResetGoal()
{
	bCompleted = false;
}

void AGSFinishGoal::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor && OtherActor->IsA(APawn::StaticClass()))
	{
		TryComplete();
	}
}
