// GravityShift v5 - captures and restores transform, velocities, collision and physics state.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSResettableComponent.generated.h"

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Resettable"))
class GRAVITYSHIFT_API UGSResettableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSResettableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bCaptureOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRestoreVisibility = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRestoreCollision = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRestorePhysicsState = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bClearRuntimeModifiers = true;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void CaptureInitialState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void CaptureCheckpointState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RestoreInitialState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RestoreCheckpointState();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void TeleportAndReset(FTransform NewTransform);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool HasCheckpointState() const;

	virtual void BeginPlay() override;

protected:
	bool bHasInitial = false;
	bool bHasCheckpoint = false;

	FTransform InitialTransform = FTransform::Identity;
	FVector InitialLinearVelocity = FVector::ZeroVector;
	FVector InitialAngularVelocity = FVector::ZeroVector;
	bool bInitialSimulatingPhysics = false;
	TEnumAsByte<ECollisionEnabled::Type> InitialCollision = ECollisionEnabled::QueryAndPhysics;
	bool bInitialHidden = false;

	FTransform CheckpointTransform = FTransform::Identity;
	FVector CheckpointLinearVelocity = FVector::ZeroVector;
	FVector CheckpointAngularVelocity = FVector::ZeroVector;
	bool bCheckpointSimulatingPhysics = false;
	TEnumAsByte<ECollisionEnabled::Type> CheckpointCollision = ECollisionEnabled::QueryAndPhysics;
	bool bCheckpointHidden = false;

	void CaptureTo(FTransform& OutTransform, FVector& OutLinear, FVector& OutAngular, bool& bOutSimulating, TEnumAsByte<ECollisionEnabled::Type>& OutCollision, bool& bOutHidden) const;
	void RestoreFrom(const FTransform& InTransform, const FVector& InLinear, const FVector& InAngular, bool bInSimulating, TEnumAsByte<ECollisionEnabled::Type> InCollision, bool bInHidden);
};
