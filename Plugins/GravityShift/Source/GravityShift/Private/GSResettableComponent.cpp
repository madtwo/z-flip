#include "GSResettableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "GSSurfaceReceiverComponent.h"

UGSResettableComponent::UGSResettableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSResettableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bCaptureOnBeginPlay)
	{
		CaptureInitialState();
		CaptureCheckpointState();
	}
}

void UGSResettableComponent::CaptureTo(FTransform& OutTransform, FVector& OutLinear, FVector& OutAngular, bool& bOutSimulating, TEnumAsByte<ECollisionEnabled::Type>& OutCollision, bool& bOutHidden) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	OutTransform = OwnerActor->GetActorTransform();
	bOutHidden = OwnerActor->IsHidden();

	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	if (!Prim)
	{
		return;
	}

	OutLinear = Prim->GetPhysicsLinearVelocity();
	OutAngular = Prim->GetPhysicsAngularVelocityInDegrees();
	bOutSimulating = Prim->IsSimulatingPhysics();
	OutCollision = Prim->GetCollisionEnabled();
}

void UGSResettableComponent::CaptureInitialState()
{
	CaptureTo(InitialTransform, InitialLinearVelocity, InitialAngularVelocity, bInitialSimulatingPhysics, InitialCollision, bInitialHidden);
	bHasInitial = true;
}

void UGSResettableComponent::CaptureCheckpointState()
{
	CaptureTo(CheckpointTransform, CheckpointLinearVelocity, CheckpointAngularVelocity, bCheckpointSimulatingPhysics, CheckpointCollision, bCheckpointHidden);
	bHasCheckpoint = true;
}

void UGSResettableComponent::RestoreFrom(const FTransform& InTransform, const FVector& InLinear, const FVector& InAngular, bool bInSimulating, TEnumAsByte<ECollisionEnabled::Type> InCollision, bool bInHidden)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	if (Prim && bRestorePhysicsState && Prim->IsSimulatingPhysics())
	{
		Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	OwnerActor->SetActorTransform(InTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (Prim)
	{
		if (bRestorePhysicsState)
		{
			Prim->SetSimulatePhysics(bInSimulating);
			if (bInSimulating)
			{
				Prim->SetPhysicsLinearVelocity(InLinear);
				Prim->SetPhysicsAngularVelocityInDegrees(InAngular);
			}
		}
		if (bRestoreCollision)
		{
			Prim->SetCollisionEnabled(InCollision);
		}
		Prim->SetEnableGravity(false);
	}

	if (bRestoreVisibility)
	{
		OwnerActor->SetActorHiddenInGame(bInHidden);
	}

	if (bClearRuntimeModifiers)
	{
		if (UGSSurfaceReceiverComponent* Receiver = OwnerActor->FindComponentByClass<UGSSurfaceReceiverComponent>())
		{
			Receiver->ClearSurfaceModifiers();
		}
	}
}

void UGSResettableComponent::RestoreInitialState()
{
	if (bHasInitial)
	{
		RestoreFrom(InitialTransform, InitialLinearVelocity, InitialAngularVelocity, bInitialSimulatingPhysics, InitialCollision, bInitialHidden);
	}
}

void UGSResettableComponent::RestoreCheckpointState()
{
	if (bHasCheckpoint)
	{
		RestoreFrom(CheckpointTransform, CheckpointLinearVelocity, CheckpointAngularVelocity, bCheckpointSimulatingPhysics, CheckpointCollision, bCheckpointHidden);
	}
	else
	{
		RestoreInitialState();
	}
}

void UGSResettableComponent::TeleportAndReset(FTransform NewTransform)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	OwnerActor->SetActorTransform(NewTransform, false, nullptr, ETeleportType::TeleportPhysics);

	UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	if (Prim)
	{
		Prim->SetPhysicsLinearVelocity(FVector::ZeroVector);
		Prim->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
}

bool UGSResettableComponent::HasCheckpointState() const
{
	return bHasCheckpoint;
}
