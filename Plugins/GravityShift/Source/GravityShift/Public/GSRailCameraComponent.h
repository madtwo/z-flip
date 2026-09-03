// GravityShift v6 - rail camera tracking.
// The player camera rides an AGSCameraRail like a ring on a rod: it slides along
// the rail's central axis with the ball, its position inside the cross-section
// stays fixed, and the view direction is rebuilt around WORLD up with small
// clamped gimbal adjustments. Gravity only enters indirectly: it moves the ball,
// and the gimbal follows the ball (floor -> pitch down, ceiling -> pitch up), so
// a flip no longer rolls the whole view 180 degrees.
// Levels place rails; this component discovers them and hands off between rails.
// No rail in the level -> the pawn keeps its original chase camera.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSRailCameraComponent.generated.h"

class AGSCameraRail;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Rail Camera"))
class GRAVITYSHIFT_API UGSRailCameraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSRailCameraComponent();

	// Gimbal limits (degrees) relative to the rail-aligned, world-up base frame.
	// Small angles by design: the camera nudges to frame the ball, it never
	// free-orbits or rolls with gravity.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "1.0"))
	float MaxYawDegrees = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "1.0"))
	float MaxPitchDegrees = 50.0f;

	// Exponential smoothing rates (per second) for the slide and the view angle.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.1"))
	float PositionLerpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.1"))
	float RotationLerpSpeed = 6.0f;

	// Leads the slide target along the rail by ball velocity so fast travel does
	// not drag the camera behind.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float LookAheadSeconds = 0.12f;

	// Keeps the camera this far BEHIND the ball along the rail (opposite the base
	// view direction), so the ball leads and the camera trails like a chase cam.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float TrailDistanceCm = 700.0f;

	// Per-press change applied by the player camera-distance keys (Q/E).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "1.0"))
	float TrailAdjustStepCm = 50.0f;

	// Hard clamps for the player-adjustable trail distance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float TrailMinCm = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float TrailMaxCm = 1400.0f;

	// Player-facing trail adjust: Direction > 0 sends the camera farther behind,
	// < 0 pulls it closer. Result clamped to [TrailMinCm, TrailMaxCm].
	UFUNCTION(BlueprintCallable, Category = "Rail Camera")
	void AdjustTrailDistance(float Direction);

	// The look target sits this far above the ball (world up) so the ball rests
	// in the lower part of the screen instead of dead center.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float AimOffsetUpCm = 40.0f;

	// Side offset of the look target along the rail-aligned right vector.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera")
	float AimOffsetRightCm = 0.0f;

	// The ball may stray this far past the rail ends before the rail stops counting.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float RailEndMarginCm = 100.0f;

	// Hysteresis: the active rail keeps priority until the ball leaves it by this
	// much past its (already extended) zone, so the camera never flickers between
	// touching rails.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rail Camera", meta = (ClampMin = "0.0"))
	float RailSwitchMarginCm = 150.0f;

	// True while the last ComputeCameraPose produced a rail pose (drives the camera).
	UFUNCTION(BlueprintPure, Category = "Rail Camera")
	bool IsDriving() const { return bActive; }

	UFUNCTION(BlueprintPure, Category = "Rail Camera")
	AGSCameraRail* GetActiveRail() const { return ActiveRail; }

	UFUNCTION(BlueprintPure, Category = "Rail Camera")
	int32 GetNumRails() const { return Rails.Num(); }

	// Computes the smoothed camera world pose for this frame. Returns false when
	// no rail applies (the caller keeps its fallback camera). Pawn-driven: call
	// once per tick from the pawn's camera update.
	bool ComputeCameraPose(FVector& OutPosition, FQuat& OutRotation, float DeltaSeconds);

protected:
	UPROPERTY()
	TObjectPtr<AGSCameraRail> ActiveRail = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<AGSCameraRail>> Rails;

	bool bRailsScanned = false;
	bool bActive = false;
	bool bHasPose = false;
	FVector SmoothedPosition = FVector::ZeroVector;
	FQuat SmoothedRotation = FQuat::Identity;

	void ScanRails();
	AGSCameraRail* SelectRail(const FVector& BallPosition);
};
