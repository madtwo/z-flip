#include "GSRailCameraComponent.h"

#include "EngineUtils.h"
#include "GSCameraRail.h"
#include "GSRollingBallPawn.h"

UGSRailCameraComponent::UGSRailCameraComponent()
{
	// The pawn calls ComputeCameraPose from its own tick; no component tick needed.
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSRailCameraComponent::ScanRails()
{
	Rails.Reset();

	UWorld* World = GetWorld();
	if (World)
	{
		for (TActorIterator<AGSCameraRail> It(World); It; ++It)
		{
			Rails.Add(*It);
		}
	}

	bRailsScanned = true;
}

AGSCameraRail* UGSRailCameraComponent::SelectRail(const FVector& BallPosition)
{
	if (!bRailsScanned || Rails.Num() == 0)
	{
		ScanRails();
	}

	Rails.RemoveAll([](const TObjectPtr<AGSCameraRail>& Rail) { return Rail == nullptr; });
	if (Rails.Num() == 0)
	{
		// Keep rescan enabled so rails added after boot (streaming, PIE restart)
		// still bind.
		bRailsScanned = false;
		ActiveRail = nullptr;
		return nullptr;
	}

	// The active rail keeps priority with an extended zone: hysteresis against
	// flickering on the boundary between touching rails.
	if (ActiveRail)
	{
		if (ActiveRail->IsPointInZone(BallPosition, RailEndMarginCm + RailSwitchMarginCm))
		{
			return ActiveRail;
		}
		ActiveRail = nullptr;
	}

	AGSCameraRail* Best = nullptr;
	float BestDistance = TNumericLimits<float>::Max();
	for (const TObjectPtr<AGSCameraRail>& Rail : Rails)
	{
		if (!Rail->IsPointInZone(BallPosition, RailEndMarginCm))
		{
			continue;
		}

		const float Distance = Rail->DistanceToAxis(BallPosition);
		if (Distance < BestDistance)
		{
			BestDistance = Distance;
			Best = Rail;
		}
	}

	ActiveRail = Best;
	return ActiveRail;
}

void UGSRailCameraComponent::AdjustTrailDistance(float Direction)
{
	TrailDistanceCm = FMath::Clamp(TrailDistanceCm + Direction * TrailAdjustStepCm, TrailMinCm, TrailMaxCm);
}

bool UGSRailCameraComponent::ComputeCameraPose(FVector& OutPosition, FQuat& OutRotation, float DeltaSeconds)
{
	const AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(GetOwner());
	if (!Ball)
	{
		return false;
	}

	// Guard the exponential smoothing against time-step spikes: PIE pause/resume
	// (e.g. remote python execution) can hand Tick a negative or huge delta, and
	// a negative alpha would extrapolate the camera AWAY from its target.
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);

	AGSCameraRail* Rail = SelectRail(Ball->GetActorLocation());
	if (!Rail)
	{
		bActive = false;
		bHasPose = false;
		return false;
	}
	bActive = true;

	const FVector Axis = Rail->GetAxisDirection();
	const FVector Origin = Rail->GetActorLocation();
	const float HalfLength = Rail->RailLength * 0.5f;

	const FVector BallLocation = Ball->GetActorLocation();
	// Base view sign: +1 looks along +axis, -1 along -axis. The camera trails the
	// ball by TrailDistanceCm opposite the view direction.
	const float ViewSign = Rail->bLookAlongNegativeAxis ? -1.0f : 1.0f;
	float TargetT = FVector::DotProduct(BallLocation - Origin, Axis)
		+ FVector::DotProduct(Ball->GetBallLinearVelocity(), Axis) * LookAheadSeconds
		- ViewSign * TrailDistanceCm;
	TargetT = FMath::Clamp(TargetT, -HalfLength, HalfLength);

	// Cross-section offset: fixed while playing. Zero keeps the camera ring
	// exactly on the axis; a positive height lifts it along world up projected
	// perpendicular to the axis (rail-local X when the axis itself is vertical).
	FVector CrossOffset = FVector::ZeroVector;
	if (Rail->CrossOffsetHeightCm > 0.0f)
	{
		FVector Perp = FVector::UpVector - Axis * FVector::DotProduct(FVector::UpVector, Axis);
		if (Perp.IsNearlyZero())
		{
			Perp = Rail->GetActorQuat().GetAxisX();
			Perp -= Axis * FVector::DotProduct(Perp, Axis);
		}
		CrossOffset = Perp.GetSafeNormal() * Rail->CrossOffsetHeightCm;
	}

	const FVector TargetPosition = Origin + Axis * TargetT + CrossOffset;

	// ---- orientation: rail-aligned base frame + clamped gimbal toward the ball ----
	// Roll reference is WORLD up, not gravity up: a gravity flip must not roll the
	// view, it only moves the ball (and with it the gimbal aim). This is the whole
	// point of the rail camera - no more 180 degree view rolls.
	const FVector Up = FVector::UpVector;

	FVector BaseForward = Axis * ViewSign;
	if (FMath::Abs(FVector::DotProduct(BaseForward, Up)) > 0.9f)
	{
		// Vertical rail: fall back to looking roughly at the ball's side.
		BaseForward = BallLocation - TargetPosition;
	}
	BaseForward -= Up * FVector::DotProduct(BaseForward, Up);
	BaseForward = BaseForward.GetSafeNormal();

	const FVector RightBase = FVector::CrossProduct(Up, BaseForward).GetSafeNormal();
	const FVector AimPoint = BallLocation
		+ Up * AimOffsetUpCm
		+ RightBase * AimOffsetRightCm;

	FQuat TargetRotation = SmoothedRotation;
	const FVector Look = AimPoint - TargetPosition;
	if (!Look.IsNearlyZero())
	{
		const FVector LookDirection = Look.GetSafeNormal();

		// Elevation of the look direction above the world-horizontal plane...
		const float Pitch0 = FMath::RadiansToDegrees(
			FMath::Asin(FMath::Clamp(FVector::DotProduct(LookDirection, Up), -1.0f, 1.0f)));

		// ...and yaw around world up relative to the rail base forward.
		FVector LookHorizontal = LookDirection - Up * FVector::DotProduct(LookDirection, Up);
		float Yaw0 = 0.0f;
		if (!LookHorizontal.IsNearlyZero())
		{
			LookHorizontal.Normalize();
			Yaw0 = FMath::RadiansToDegrees(FMath::Atan2(
				FVector::DotProduct(FVector::CrossProduct(BaseForward, LookHorizontal), Up),
				FVector::DotProduct(BaseForward, LookHorizontal)));
		}

		const float Yaw = FMath::Clamp(Yaw0, -MaxYawDegrees, MaxYawDegrees);
		const float Pitch = FMath::Clamp(Pitch0, -MaxPitchDegrees, MaxPitchDegrees);

		const FQuat YawQuat(Up, FMath::DegreesToRadians(Yaw));
		const FVector Forward = YawQuat.RotateVector(BaseForward);
		const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();
		const float PitchRad = FMath::DegreesToRadians(Pitch);
		const FVector FinalForward = (Forward * FMath::Cos(PitchRad) + Up * FMath::Sin(PitchRad)).GetSafeNormal();
		const FVector FinalUp = FVector::CrossProduct(FinalForward, Right).GetSafeNormal();

		if (!FinalForward.IsNearlyZero() && !FinalUp.IsNearlyZero())
		{
			TargetRotation = FRotationMatrix::MakeFromXZ(FinalForward, FinalUp).ToQuat();
		}
	}

	if (!bHasPose)
	{
		SmoothedPosition = TargetPosition;
		SmoothedRotation = TargetRotation;
		bHasPose = true;
	}
	else
	{
		const float PosAlpha = 1.0f - FMath::Exp(-FMath::Max(PositionLerpSpeed, 0.1f) * DeltaSeconds);
		const float RotAlpha = 1.0f - FMath::Exp(-FMath::Max(RotationLerpSpeed, 0.1f) * DeltaSeconds);
		SmoothedPosition = FMath::Lerp(SmoothedPosition, TargetPosition, PosAlpha);
		SmoothedRotation = FQuat::Slerp(SmoothedRotation, TargetRotation, RotAlpha);
	}

	OutPosition = SmoothedPosition;
	OutRotation = SmoothedRotation;
	return true;
}
