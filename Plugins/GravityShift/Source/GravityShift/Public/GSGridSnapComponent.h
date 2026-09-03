// GravityShift v6 - snaps an owning actor to an axis-aligned grid.
// Blocks only (the ball never carries one). Snapping is suspended while a gravity
// flip is settling and while the body is moving fast, so it never fights physics.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "GravityShiftTypes.h"
#include "GSGridSnapComponent.generated.h"

class AGSGravityManager;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Grid Snap"))
class GRAVITYSHIFT_API UGSGridSnapComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSGridSnapComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Snap")
	bool bSnapEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Snap", meta = (ClampMin = "0.01"))
	float GridSize = 100.0f;

	// How long after a gravity change snap stays disabled (matches the camera flip).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Snap", meta = (ClampMin = "0.0"))
	float SnapRestoreDelaySeconds = 0.35f;

	// Below this speed a simulated body is treated as settled and may be snapped.
	// Above it snapping is skipped so a block travelling between surfaces is not yanked.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid Snap", meta = (ClampMin = "0.0"))
	float SnapMaxMoveSpeedCm = 60.0f;

	UFUNCTION(BlueprintCallable, Category = "Grid Snap")
	FVector SnapToGrid(const FVector& InLocation) const;

	// Immediately moves the owner to the nearest grid point.
	UFUNCTION(BlueprintCallable, Category = "Grid Snap")
	void ApplySnap();

	UFUNCTION(BlueprintCallable, Category = "Grid Snap")
	void SetSnapEnabled(bool bEnabled);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY()
	TObjectPtr<AGSGravityManager> GravityManager = nullptr;

	FTimerHandle ReenableTimerHandle;

	UFUNCTION()
	void HandleGravityDirectionChanged(EGSGravityDirection NewDirection, FVector DirectionVector, int32 Revision,
		EGSGravityChangeReason Reason, AActor* Requester);

	void ReenableSnapAfterFlip();
	bool IsSafeToSnap() const;
	void TrySubscribeToManager();
};
