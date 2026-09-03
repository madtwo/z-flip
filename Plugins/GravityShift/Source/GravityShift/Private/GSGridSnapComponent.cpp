#include "GSGridSnapComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GSGravityManager.h"

UGSGridSnapComponent::UGSGridSnapComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Run after the physics sub-step so snapping doesn't feed a stale transform
	// back into the solve of the same frame.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UGSGridSnapComponent::BeginPlay()
{
	Super::BeginPlay();
	TrySubscribeToManager();

	if (bSnapEnabled)
	{
		ApplySnap();
	}
}

void UGSGridSnapComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GravityManager)
	{
		GravityManager->OnGravityDirectionChanged.RemoveDynamic(this, &UGSGridSnapComponent::HandleGravityDirectionChanged);
	}

	UWorld* World = GetWorld();
	if (World)
	{
		World->GetTimerManager().ClearTimer(ReenableTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UGSGridSnapComponent::TrySubscribeToManager()
{
	if (GravityManager)
	{
		return;
	}

	GravityManager = AGSGravityManager::FindGravityManager(this);
	if (GravityManager)
	{
		GravityManager->OnGravityDirectionChanged.AddDynamic(this, &UGSGridSnapComponent::HandleGravityDirectionChanged);
	}
}

void UGSGridSnapComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GravityManager)
	{
		// The manager may be game-mode-spawned after level-placed actors begin.
		TrySubscribeToManager();
	}

	if (!bSnapEnabled)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || GridSize <= 0.01f)
	{
		return;
	}

	if (!IsSafeToSnap())
	{
		return;
	}

	Owner->SetActorLocation(SnapToGrid(Owner->GetActorLocation()), /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
}

FVector UGSGridSnapComponent::SnapToGrid(const FVector& InLocation) const
{
	if (GridSize <= 0.01f)
	{
		return InLocation;
	}

	return FVector(
		FMath::RoundToFloat(InLocation.X / GridSize) * GridSize,
		FMath::RoundToFloat(InLocation.Y / GridSize) * GridSize,
		FMath::RoundToFloat(InLocation.Z / GridSize) * GridSize);
}

void UGSGridSnapComponent::ApplySnap()
{
	AActor* Owner = GetOwner();
	if (!Owner || GridSize <= 0.01f)
	{
		return;
	}

	const FVector Snapped = SnapToGrid(Owner->GetActorLocation());
	if (!Snapped.Equals(Owner->GetActorLocation(), 0.1f))
	{
		Owner->SetActorLocation(Snapped, /*bSweep=*/false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UGSGridSnapComponent::SetSnapEnabled(bool bEnabled)
{
	bSnapEnabled = bEnabled;
}

void UGSGridSnapComponent::HandleGravityDirectionChanged(EGSGravityDirection NewDirection, FVector DirectionVector,
	int32 Revision, EGSGravityChangeReason Reason, AActor* Requester)
{
	// Only self-manage the suspension when snapping is actually on; a designer who
	// disabled it keeps it disabled.
	if (!bSnapEnabled)
	{
		return;
	}

	// Suspend snapping while the flip settles, then resume and align.
	bSnapEnabled = false;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ReenableTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(ReenableTimerHandle);
	}
	World->GetTimerManager().SetTimer(ReenableTimerHandle, this,
		&UGSGridSnapComponent::ReenableSnapAfterFlip, FMath::Max(SnapRestoreDelaySeconds, 0.001f), false);
}

void UGSGridSnapComponent::ReenableSnapAfterFlip()
{
	bSnapEnabled = true;

	// If the body is still travelling to its new surface, leave it to the tick
	// gate; only snap once it has settled.
	if (IsSafeToSnap())
	{
		ApplySnap();
	}
}

bool UGSGridSnapComponent::IsSafeToSnap() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(Owner->GetRootComponent());
	if (!Root || !Root->IsSimulatingPhysics())
	{
		return true;
	}

	const FVector Velocity = Root->GetPhysicsLinearVelocity();
	return Velocity.Size() <= SnapMaxMoveSpeedCm;
}
