// GravityShift - one-way wall -> floor redirect volume with local convex-arc assist.

#include "GSOneWayGravityRedirectVolume.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GSRollingBallPawn.h"
#include "GSLandingResponseComponent.h"

AGSOneWayGravityRedirectVolume::AGSOneWayGravityRedirectVolume()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	SetRootComponent(TriggerBox);
	TriggerBox->InitBoxExtent(FVector(220.0f, 190.0f, 220.0f));
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TriggerBox->SetGenerateOverlapEvents(false);
	TriggerBox->SetHiddenInGame(true);

	ArcAssistBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ArcAssistBox"));
	ArcAssistBox->SetupAttachment(TriggerBox);
	// Tuned relative to the supplied current TriggerBox center (3550,-1330,-400).
	ArcAssistBox->SetRelativeLocation(FVector(0.0f, 82.0f, -160.0f));
	ArcAssistBox->InitBoxExtent(FVector(230.0f, 95.0f, 120.0f));
	ArcAssistBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ArcAssistBox->SetGenerateOverlapEvents(false);
	ArcAssistBox->SetHiddenInGame(true);
}

void AGSOneWayGravityRedirectVolume::BeginPlay()
{
	Super::BeginPlay();

	RefreshArcAssistSurfaces();

	if (bDebugLog)
	{
		const FVector ArmedA = GetWorldGravityA();
		const FVector ArmedB = GetWorldGravityB();
		UE_LOG(LogTemp, Log,
			TEXT("[GSOneWayRedirect] %s armed mode=%s entry=%s target=%s A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f) center=%s extent=%s centerOnly=%d assistSurfaces=%d arcTag=%s exitDot=%.2f"),
			*GetNameSafe(this),
			bBidirectional ? TEXT("two-way-face-capture") : TEXT("one-way"),
			*GSGravity::GetDirectionDisplayName(EntryGravityDirection),
			*GSGravity::GetDirectionDisplayName(TargetGravityDirection),
			ArmedA.X, ArmedA.Y, ArmedA.Z,
			ArmedB.X, ArmedB.Y, ArmedB.Z,
			*TriggerBox->GetComponentLocation().ToString(),
			*TriggerBox->GetScaledBoxExtent().ToString(),
			bExpandTriggerByBallRadius ? 0 : 1,
			ArcAssistSurfaces.Num(),
			*ArcAssistSurfaceTag.ToString(),
			FMath::Max(TwoWayArcProgressMinDot, TwoWayArcExitNormalMinDot));
	}
}

bool AGSOneWayGravityRedirectVolume::IsBallInsideBox(
	const UBoxComponent& Box,
	const USphereComponent& BallSphere,
	bool bExpandByBallRadius,
	float ExtraPaddingCm) const
{
	const FVector BallLocation = BallSphere.GetComponentLocation();
	const FVector LocalNoScale = Box.GetComponentTransform().InverseTransformPositionNoScale(BallLocation);
	const FVector Extent = Box.GetScaledBoxExtent();
	const float RadiusMargin = bExpandByBallRadius ? BallSphere.GetScaledSphereRadius() : 0.0f;
	const float Margin = RadiusMargin + FMath::Max(ExtraPaddingCm, 0.0f);

	return FMath::Abs(LocalNoScale.X) <= Extent.X + Margin
		&& FMath::Abs(LocalNoScale.Y) <= Extent.Y + Margin
		&& FMath::Abs(LocalNoScale.Z) <= Extent.Z + Margin;
}

// [2026-09-29 项目侧扩展] 双向模式的三个工具函数。
FVector AGSOneWayGravityRedirectVolume::GetWorldGravityA() const
{
	return GetActorTransform().TransformVectorNoScale(LocalGravityADirection).GetSafeNormal();
}

FVector AGSOneWayGravityRedirectVolume::GetWorldGravityB() const
{
	return GetActorTransform().TransformVectorNoScale(LocalGravityBDirection).GetSafeNormal();
}

FVector AGSOneWayGravityRedirectVolume::GetWorldArcAssistAxis() const
{
	// v3: placement-driven means the probe/rail axis must rotate with the volume too.
	// Keep one-way mode byte-for-byte compatible in behavior: there ArcAssistAxis remains world-space.
	const FVector Axis = bBidirectional
		? GetActorTransform().TransformVectorNoScale(ArcAssistAxis)
		: ArcAssistAxis;
	return Axis.GetSafeNormal();
}

bool AGSOneWayGravityRedirectVolume::ResolveTwoWayGravity(const FVector& CurrentGravity, FVector& OutEntry, FVector& OutExit) const
{
	const FVector FaceA = GetWorldGravityA();
	const FVector FaceB = GetWorldGravityB();
	if (FaceA.IsNearlyZero() || FaceB.IsNearlyZero() || CurrentGravity.IsNearlyZero())
	{
		return false;
	}
	if (FMath::Abs(FVector::DotProduct(FaceA, FaceB)) > TwoWayPerpendicularTolerance)
	{
		// 两面不垂直 = 摆放配置错误。fail-closed:不触发,留给关卡侧修正。
		return false;
	}
	const float DotA = FVector::DotProduct(CurrentGravity, FaceA);
	const float DotB = FVector::DotProduct(CurrentGravity, FaceB);
	if (DotA >= EntryGravityMinDot)
	{
		OutEntry = FaceA;
		OutExit = FaceB;
		return true;
	}
	if (DotB >= EntryGravityMinDot)
	{
		OutEntry = FaceB;
		OutExit = FaceA;
		return true;
	}
	// 球的重力不落在任何一面上(空中 / 别的姿态)→ 不触发。
	return false;
}

bool AGSOneWayGravityRedirectVolume::PassesTwoWayEntryGates(
	AGSRollingBallPawn& Ball,
	const USphereComponent& BallSphere,
	const FVector& EntryGravity,
	const FVector& ExitGravity)
{
	if (!bUseTwoWayEntryGates)
	{
		return true;
	}

	auto Reject = [&](const TCHAR* Reason, float Value = 0.0f, float Limit = 0.0f)
	{
		if (bDebugLog)
		{
			UE_LOG(LogTemp, VeryVerbose,
				TEXT("[GSOneWayRedirect] %s gate[%s] value=%.3f limit=%.3f"),
				*GetNameSafe(this), Reason, Value, Limit);
		}
		return false;
	};

	const FVector Velocity = Ball.GetBallLinearVelocity();
	const float Speed = Velocity.Size();
	if (Speed < TwoWayMinTriggerSpeedCm)
	{
		return Reject(TEXT("speed"), Speed, TwoWayMinTriggerSpeedCm);
	}

	if (bTwoWayRequireSupport && Ball.LandingResponse)
	{
		const float AirborneSeconds = Ball.LandingResponse->GetAirborneSeconds();
		if (!Ball.LandingResponse->IsSupported() || AirborneSeconds > TwoWayMaxAirborneSeconds)
		{
			return Reject(TEXT("support"), AirborneSeconds, TwoWayMaxAirborneSeconds);
		}
	}

	if (!bTwoWayRequireArcContact)
	{
		return true;
	}

	if (ArcAssistSurfaces.IsEmpty())
	{
		RefreshArcAssistSurfaces();
	}

	FHitResult SurfaceHit;
	FVector InwardDirection = FVector::ZeroVector;
	if (!FindArcAssistSurfaceHit(BallSphere, SurfaceHit, InwardDirection))
	{
		return Reject(TEXT("arc-contact"));
	}

	const float Radius = BallSphere.GetScaledSphereRadius();
	const float SurfaceGapCm = SurfaceHit.Distance - Radius;
	if (SurfaceGapCm > TwoWayArcContactMarginCm)
	{
		return Reject(TEXT("arc-gap"), SurfaceGapCm, TwoWayArcContactMarginCm);
	}

	FVector ContactNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (ContactNormal.IsNearlyZero())
	{
		ContactNormal = SurfaceHit.Normal.GetSafeNormal();
	}
	if (ContactNormal.IsNearlyZero())
	{
		return Reject(TEXT("arc-normal"));
	}

	const FVector EntryUp = -EntryGravity.GetSafeNormal();
	const FVector ExitUp = -ExitGravity.GetSafeNormal();
	if (EntryUp.IsNearlyZero() || ExitUp.IsNearlyZero())
	{
		return Reject(TEXT("face-axis"));
	}

	const FVector RailAxis = GetWorldArcAssistAxis();
	if (!RailAxis.IsNearlyZero())
	{
		const float LateralNormalDot = FMath::Abs(FVector::DotProduct(ContactNormal, RailAxis));
		if (LateralNormalDot > TwoWayMaxLateralNormalDot)
		{
			return Reject(TEXT("side"), LateralNormalDot, TwoWayMaxLateralNormalDot);
		}
	}

	const float EntryNormalDot = FVector::DotProduct(ContactNormal, EntryUp);
	const float ExitNormalDot = FVector::DotProduct(ContactNormal, ExitUp);
	if (EntryNormalDot < -TwoWayArcFaceNormalTolerance || ExitNormalDot < -TwoWayArcFaceNormalTolerance)
	{
		return Reject(TEXT("arc-face"), FMath::Min(EntryNormalDot, ExitNormalDot), -TwoWayArcFaceNormalTolerance);
	}

	// v5 ARC-ONLY: switch gravity near the EXIT face, not merely just after entering the arc.
	// This is the critical bidirectional fix. Early switching is asymmetric on a convex quarter arc:
	// in wall->floor travel, changing to -Z near the wall creates a tangent acceleration back toward
	// the wall. Waiting until ContactNormal is close to ExitUp lets the old gravity carry the ball around
	// the arc and applies the new gravity only when it can press the ball onto the exit support surface.
	// Keep the legacy serialized threshold as a lower bound, but never allow it to weaken the new exit gate.
	const float ExitNormalMinDot = FMath::Clamp(
		FMath::Max(TwoWayArcProgressMinDot, TwoWayArcExitNormalMinDot),
		0.0f,
		1.0f);
	if (ExitNormalDot < ExitNormalMinDot)
	{
		return Reject(TEXT("arc-exit"), ExitNormalDot, ExitNormalMinDot);
	}

	// Placement-independent direction gate. Project the EXIT support normal onto the current tangent plane;
	// that is the local surface direction that advances the contact normal toward the exit face. This works
	// for both convex directions, unlike comparing velocity directly with ExitGravity.
	const FVector ApproachDirection = FVector::VectorPlaneProject(ExitUp, ContactNormal).GetSafeNormal();
	if (!ApproachDirection.IsNearlyZero())
	{
		const float ApproachSpeedCm = FVector::DotProduct(Velocity, ApproachDirection);
		if (ApproachSpeedCm < TwoWayMinApproachSpeedCm)
		{
			return Reject(TEXT("approach"), ApproachSpeedCm, TwoWayMinApproachSpeedCm);
		}
	}

	return true;
}

bool AGSOneWayGravityRedirectVolume::TryBeginTwoWayFaceCapture(AGSRollingBallPawn& Ball)
{
	// v7: the bevel contact itself owns the handoff moment. TriggerBox is not an entry prerequisite.
	if (Ball.IsGravityRedirecting() || Ball.IsFaceCapturing() || Ball.IsFaceCaptureCoolingDown())
	{
		return false;
	}

	USphereComponent* BallSphere = Ball.GetBallCollisionComponent();
	if (!BallSphere || !BallSphere->IsSimulatingPhysics())
	{
		return false;
	}

	const FVector Current = Ball.GetActiveGravityDirection().GetSafeNormal();
	FVector Entry = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	if (!ResolveTwoWayGravity(Current, Entry, Target))
	{
		return false;
	}

	if (FVector::DotProduct(Current, Target) >= TargetAlreadyMinDot)
	{
		return false;
	}

	if (ArcAssistSurfaces.IsEmpty())
	{
		RefreshArcAssistSurfaces();
	}

	FHitResult SurfaceHit;
	FVector InwardDirection = FVector::ZeroVector;
	if (!FindArcAssistSurfaceHit(*BallSphere, SurfaceHit, InwardDirection))
	{
		return false;
	}

	AActor* HitArc = SurfaceHit.GetActor();
	if (!HitArc || (!ArcAssistSurfaceTag.IsNone() && !HitArc->ActorHasTag(ArcAssistSurfaceTag)))
	{
		return false;
	}

	const float Radius = BallSphere->GetScaledSphereRadius();
	const float SurfaceGapCm = SurfaceHit.Distance - Radius;
	if (SurfaceGapCm > TwoWayArcContactMarginCm)
	{
		return false;
	}

	FVector ContactNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
	if (ContactNormal.IsNearlyZero())
	{
		ContactNormal = SurfaceHit.Normal.GetSafeNormal();
	}
	if (ContactNormal.IsNearlyZero())
	{
		return false;
	}

	const FVector EntryUp = -Entry.GetSafeNormal();
	const FVector ExitUp = -Target.GetSafeNormal();
	if (EntryUp.IsNearlyZero() || ExitUp.IsNearlyZero())
	{
		return false;
	}

	// Never capture a side wall / rail cap. The contact normal must belong to the bevel plane.
	const FVector RailAxis = GetWorldArcAssistAxis();
	if (!RailAxis.IsNearlyZero()
		&& FMath::Abs(FVector::DotProduct(ContactNormal, RailAxis)) > TwoWayMaxLateralNormalDot)
	{
		return false;
	}

	// The live normal must lie in the quadrant connecting the two support normals. This rejects the
	// back/bottom of the mesh without delaying entry until some arbitrary progress value.
	const float EntryNormalDot = FVector::DotProduct(ContactNormal, EntryUp);
	const float ExitNormalDot = FVector::DotProduct(ContactNormal, ExitUp);
	if (EntryNormalDot < -TwoWayArcFaceNormalTolerance || ExitNormalDot < -TwoWayArcFaceNormalTolerance)
	{
		return false;
	}

	const FVector Velocity = Ball.GetBallLinearVelocity();
	const FVector ApproachDirection = FVector::VectorPlaneProject(ExitUp, ContactNormal).GetSafeNormal();
	const float ApproachSpeedCm = ApproachDirection.IsNearlyZero()
		? 0.0f
		: FVector::DotProduct(Velocity, ApproachDirection);

	// Contact itself is authoritative. A slow ball must still be catchable. We only reject an
	// unambiguous opposite-direction pass, which avoids the old "gate rejected the only contact frame" bug.
	if (!ApproachDirection.IsNearlyZero() && ApproachSpeedCm < -FMath::Max(TwoWayFaceCaptureRejectAwaySpeedCm, 0.0f))
	{
		return false;
	}

	// Support is no longer an entry hard-gate. At a convex lip IsSupported can flicker false on the exact
	// frame where the ball first touches the bevel. It only selects gentle vs hard FaceCapture behavior.
	bool bGentleCapture = true;
	if (Ball.LandingResponse)
	{
		bGentleCapture = Ball.LandingResponse->IsSupported()
			|| Ball.LandingResponse->GetAirborneSeconds() <= TwoWayMaxAirborneSeconds;
	}

	Ball.BeginFaceCapture(
		HitArc,
		ContactNormal,
		Target,
		TwoWayFaceCaptureDriveSpeedCm,
		TwoWayFaceCaptureStickAccelCm,
		TwoWayFaceCaptureExitNormalDot,
		bGentleCapture,
		TwoWayFaceCaptureGroundMinSpeedCm,
		TwoWayFaceCaptureGroundMaxSpeedCm);

	if (!Ball.IsFaceCapturing())
	{
		return false;
	}

	bRuntimeEntryResolved = true;
	RuntimeEntryGravity = Entry;
	RuntimeExitGravity = Target;
	RuntimeExitGravityEnum = GSGravity::VectorToDirection(Target);
	ActiveBall = &Ball;
	bOwnsRedirect = false; // Pawn owns FaceCapture and releases it from live surface normals.
	bConsumedUntilExit = true;
	++FireSerial;
	ResetArcAssistState();

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[GSOneWayRedirect] %s face-capture#%u arc=%s gap=%.1f gentle=%d approach=%.1f current=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f)"),
			*GetNameSafe(this),
			FireSerial,
			*GetNameSafe(HitArc),
			SurfaceGapCm,
			bGentleCapture ? 1 : 0,
			ApproachSpeedCm,
			Current.X, Current.Y, Current.Z,
			Target.X, Target.Y, Target.Z,
			ContactNormal.X, ContactNormal.Y, ContactNormal.Z);
	}

	return true;
}

bool AGSOneWayGravityRedirectVolume::TryBeginRedirect(AGSRollingBallPawn& Ball)
{
	if (bBidirectional)
	{
		return TryBeginTwoWayFaceCapture(Ball);
	}

	if (Ball.IsGravityRedirecting() || Ball.IsFaceCapturing())
	{
		// Never steal the pawn from either redirect system.
		return false;
	}

	const FVector Current = Ball.GetActiveGravityDirection().GetSafeNormal();
	if (Current.IsNearlyZero())
	{
		return false;
	}

	FVector Entry = FVector::ZeroVector;
	FVector Target = FVector::ZeroVector;
	if (bBidirectional)
	{
		// 双向:入口面由球当前重力识别,出口就是另一面 —— 与 UGSRedirectorComponent 同一规则,
		// 所以"两边都行、两边都能换重力"是同一段判定的自然结果。
		if (!ResolveTwoWayGravity(Current, Entry, Target))
		{
			if (bDebugLog)
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("[GSOneWayRedirect] %s two-way: current gravity matches neither face A=(%.2f,%.2f,%.2f) B=(%.2f,%.2f,%.2f)"),
					*GetNameSafe(this),
					GetWorldGravityA().X, GetWorldGravityA().Y, GetWorldGravityA().Z,
					GetWorldGravityB().X, GetWorldGravityB().Y, GetWorldGravityB().Z);
			}
			return false;
		}
	}
	else
	{
		Entry = GSGravity::DirectionToVector(EntryGravityDirection).GetSafeNormal();
		Target = GSGravity::DirectionToVector(TargetGravityDirection).GetSafeNormal();

		if (Entry.IsNearlyZero() || Target.IsNearlyZero())
		{
			return false;
		}

		if (FMath::Abs(FVector::DotProduct(Entry, Target)) > 0.1f)
		{
			if (bDebugLog)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[GSOneWayRedirect] %s rejected setup: entry/target are not perpendicular"),
					*GetNameSafe(this));
			}
			return false;
		}
	}

	const float EntryDot = FVector::DotProduct(Current, Entry);
	const float TargetDot = FVector::DotProduct(Current, Target);
	if (TargetDot >= TargetAlreadyMinDot || EntryDot < EntryGravityMinDot)
	{
		return false;
	}

	if (bBidirectional)
	{
		USphereComponent* BallSphere = Ball.GetBallCollisionComponent();
		if (!BallSphere || !PassesTwoWayEntryGates(Ball, *BallSphere, Entry, Target))
		{
			return false;
		}
	}

	bRuntimeEntryResolved = true;
	RuntimeEntryGravity = Entry;
	RuntimeExitGravity = Target;
	RuntimeExitGravityEnum = GSGravity::VectorToDirection(Target);

	const FVector EntryUp = -Entry;
	const FVector TargetUp = -Target;
	const FVector BendAxis = FVector::CrossProduct(EntryUp, TargetUp).GetSafeNormal();
	if (BendAxis.IsNearlyZero())
	{
		return false;
	}

	// [2026-09-26 项目侧 AI 修正] 现场实测:本关这一带的滑行切线会被"弧下沿(法线 −Z)"读成 +Y,
	// 把本来会自己落到平台上的球一把推回坑里(用户原话:"换完重力自己就往反方向滚")。
	// 改法:BeginGravityRedirect 之后**立刻 EndGravityRedirect()** —— 按 Pawn 的契约,End 会把未走完的
	// 旋转补完并提交,于是重力瞬时切到 Target、球保留自己的动量(实测切换瞬间它正漂在平台上方
	// y≈−1330 / z≈−450,自然落到平台上)。
	// 想恢复专家原行为:删掉紧跟的 Ball.EndGravityRedirect(); 这一行,并把下面的 bOwnsRedirect 改回 true。
	Ball.BeginGravityRedirect(Target, RideSpeedCm, BendAxis, RidePathLengthCm);
	Ball.EndGravityRedirect();

	ActiveBall = &Ball;
	bOwnsRedirect = false;
	bConsumedUntilExit = true;
	++FireSerial;
	ResetArcAssistState();

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[GSOneWayRedirect] %s fired#%u(instant,%s) t=%.3f current=(%.2f,%.2f,%.2f) entryDot=%.3f target=%s axis=(%.2f,%.2f,%.2f) ride=%.0f path=%.0f"),
			*GetNameSafe(this),
			FireSerial,
			bBidirectional ? TEXT("two-way") : TEXT("one-way"),
			GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f,
			Current.X, Current.Y, Current.Z,
			EntryDot,
			*GSGravity::GetDirectionDisplayName(RuntimeExitGravityEnum),
			BendAxis.X, BendAxis.Y, BendAxis.Z,
			RideSpeedCm, RidePathLengthCm);
	}

	return true;
}

void AGSOneWayGravityRedirectVolume::ClearOwnedRedirect(bool bKeepConsumedLatch)
{
	ActiveBall.Reset();
	bOwnsRedirect = false;
	if (!bKeepConsumedLatch)
	{
		bConsumedUntilExit = false;
	}
}

void AGSOneWayGravityRedirectVolume::RefreshArcAssistSurfaces()
{
	ArcAssistSurfaces.Reset();
	if (!GetWorld())
	{
		return;
	}

	// v5 ARC-ONLY: in two-way mode, a configured tag is authoritative. This makes the curved surface
	// set explicit instead of discovering every actor whose generated UObject name happens to contain
	// "Blockout_Corner_Curved". The installer tags exactly the three production arc actors.
	const bool bUseStrictTag = bBidirectional && !ArcAssistSurfaceTag.IsNone();

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == this)
		{
			continue;
		}

		const bool bMatches = bUseStrictTag
			? Candidate->ActorHasTag(ArcAssistSurfaceTag)
			: (!ArcAssistSurfaceNameContains.IsEmpty()
				&& Candidate->GetName().Contains(ArcAssistSurfaceNameContains));

		if (bMatches)
		{
			ArcAssistSurfaces.Add(Candidate);
		}
	}
}

bool AGSOneWayGravityRedirectVolume::FindArcAssistSurfaceHit(
	const USphereComponent& BallSphere,
	FHitResult& OutHit,
	FVector& OutInwardDirection) const
{
	if (ArcAssistSurfaces.IsEmpty())
	{
		return false;
	}

	const FVector Axis = GetWorldArcAssistAxis();
	if (Axis.IsNearlyZero())
	{
		return false;
	}

	FVector BasisU = FVector::UpVector;
	if (FMath::Abs(FVector::DotProduct(BasisU, Axis)) > 0.95f)
	{
		BasisU = FVector::RightVector;
	}
	BasisU = FVector::VectorPlaneProject(BasisU, Axis).GetSafeNormal();
	const FVector BasisV = FVector::CrossProduct(Axis, BasisU).GetSafeNormal();
	if (BasisU.IsNearlyZero() || BasisV.IsNearlyZero())
	{
		return false;
	}

	const FVector Start = BallSphere.GetComponentLocation();
	const float TraceDistance = BallSphere.GetScaledSphereRadius() + FMath::Max(ArcAssistProbeReachCm, 10.0f);
	const int32 RayCount = FMath::Clamp(ArcAssistProbeRays, 8, 64);
	const FCollisionQueryParams Params(SCENE_QUERY_STAT(GSArcAssistProbe), false, this);

	bool bFound = false;
	float BestDistance = TNumericLimits<float>::Max();
	FVector BestDirection = FVector::ZeroVector;
	FHitResult BestHit;

	for (int32 RayIndex = 0; RayIndex < RayCount; ++RayIndex)
	{
		const float Angle = (2.0f * PI * static_cast<float>(RayIndex)) / static_cast<float>(RayCount);
		const FVector ProbeDirection = (BasisU * FMath::Cos(Angle) + BasisV * FMath::Sin(Angle)).GetSafeNormal();
		const FVector End = Start + ProbeDirection * TraceDistance;

		for (const TWeakObjectPtr<AActor>& SurfacePtr : ArcAssistSurfaces)
		{
			AActor* SurfaceActor = SurfacePtr.Get();
			if (!SurfaceActor)
			{
				continue;
			}

			TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
			SurfaceActor->GetComponents(PrimitiveComponents);
			for (UPrimitiveComponent* Primitive : PrimitiveComponents)
			{
				if (!Primitive)
				{
					continue;
				}

				FHitResult Hit;
				if (Primitive->LineTraceComponent(Hit, Start, End, Params) && Hit.Distance < BestDistance)
				{
					BestDistance = Hit.Distance;
					BestDirection = ProbeDirection;
					BestHit = Hit;
					bFound = true;
				}
			}
		}
	}

	if (!bFound)
	{
		return false;
	}

	OutHit = BestHit;
	// ProbeDirection points from ball center toward the configured curved mesh, i.e. inward.
	OutInwardDirection = BestDirection;
	return true;
}

void AGSOneWayGravityRedirectVolume::ResetArcAssistState()
{
	LastArcAssistTangent = FVector::ZeroVector;
}

void AGSOneWayGravityRedirectVolume::UpdateArcAssist(AGSRollingBallPawn& Ball, float DeltaSeconds)
{
	if (!bArcAssistEnabled || !ArcAssistBox || Ball.IsGravityRedirecting() || Ball.IsFaceCapturing())
	{
		ResetArcAssistState();
		return;
	}

	// v5 ARC-ONLY: once this pass has changed gravity, do NOT reinterpret the new gravity as an
	// opposite-direction entry while the ball is still inside the same corridor. That was causing the
	// tangent guide to reverse and pull a successful floor->wall handoff back toward the floor.
	if (bBidirectional && bSuppressArcAssistAfterRedirectUntilExit && bConsumedUntilExit)
	{
		ResetArcAssistState();
		return;
	}

	if (ArcAssistSurfaces.IsEmpty())
	{
		RefreshArcAssistSurfaces();
	}

	USphereComponent* BallSphere = Ball.GetBallCollisionComponent();
	if (!BallSphere || !BallSphere->IsSimulatingPhysics())
	{
		ResetArcAssistState();
		return;
	}

	const FVector CurrentGravity = Ball.GetActiveGravityDirection().GetSafeNormal();
	if (CurrentGravity.IsNearlyZero())
	{
		ResetArcAssistState();
		return;
	}

	FVector AssistExitGravity = FVector::ZeroVector;
	if (bBidirectional)
	{
		// 双向模式:球在两面中的**任一面**上都可获得吸附辅助。
		FVector AssistEntryGravity = FVector::ZeroVector;
		if (!ResolveTwoWayGravity(CurrentGravity, AssistEntryGravity, AssistExitGravity))
		{
			ResetArcAssistState();
			return;
		}
	}
	else
	{
		const FVector RequiredGravity = GSGravity::DirectionToVector(ArcAssistGravityDirection).GetSafeNormal();
		if (RequiredGravity.IsNearlyZero()
			|| FVector::DotProduct(CurrentGravity, RequiredGravity) < ArcAssistGravityMinDot)
		{
			ResetArcAssistState();
			return;
		}
	}

	// The assist region is intentionally center-based. Its extents are already generous around the lip.
	if (!IsBallInsideBox(*ArcAssistBox, *BallSphere, false, 0.0f))
	{
		ResetArcAssistState();
		return;
	}

	FHitResult SurfaceHit;
	FVector InwardDirection = FVector::ZeroVector;
	if (!FindArcAssistSurfaceHit(*BallSphere, SurfaceHit, InwardDirection))
	{
		ResetArcAssistState();
		return;
	}

	FVector Velocity = BallSphere->GetPhysicsLinearVelocity();
	FVector Tangent = FVector::ZeroVector;
	if (bBidirectional)
	{
		FVector ContactNormal = SurfaceHit.ImpactNormal.GetSafeNormal();
		if (ContactNormal.IsNearlyZero())
		{
			ContactNormal = SurfaceHit.Normal.GetSafeNormal();
		}
		// v3: guide toward the other face using only geometry. Projecting ExitUp onto the current
		// tangent plane gives the correct direction for floor->wall AND wall->floor on a convex quarter arc.
		const FVector ExitUp = -AssistExitGravity.GetSafeNormal();
		Tangent = FVector::VectorPlaneProject(ExitUp, ContactNormal).GetSafeNormal();
	}
	else
	{
		const FVector Axis = GetWorldArcAssistAxis();
		Tangent = FVector::CrossProduct(Axis, InwardDirection).GetSafeNormal();
		if (!Tangent.IsNearlyZero())
		{
			if (!LastArcAssistTangent.IsNearlyZero())
			{
				if (FVector::DotProduct(Tangent, LastArcAssistTangent) < 0.0f)
				{
					Tangent *= -1.0f;
				}
			}
			else
			{
				const float Along = FVector::DotProduct(Velocity, Tangent);
				if (FMath::Abs(Along) > 20.0f)
				{
					if (Along < 0.0f)
					{
						Tangent *= -1.0f;
					}
				}
				else if (Tangent.Z < 0.0f)
				{
					Tangent *= -1.0f;
				}
			}
		}
	}

	if (Tangent.IsNearlyZero())
	{
		return;
	}
	LastArcAssistTangent = Tangent;

	const float Radius = BallSphere->GetScaledSphereRadius();
	const float GapCm = FMath::Max(SurfaceHit.Distance - Radius, 0.0f);
	const float TangentSpeedCm = FMath::Max(FVector::DotProduct(Velocity, Tangent), 0.0f);
	const float PathRadius = FMath::Max(ArcAssistPathRadiusCm, 10.0f);
	const float CentripetalDemand = (TangentSpeedCm * TangentSpeedCm) / PathRadius;
	const float InwardAccel = FMath::Clamp(
		ArcAssistBaseAccelCm
			+ ArcAssistCentripetalScale * CentripetalDemand
			+ ArcAssistGapGain * GapCm,
		0.0f,
		FMath::Max(ArcAssistMaxAccelCm, 0.0f));

	if (InwardAccel > 0.0f)
	{
		BallSphere->AddForce(InwardDirection * InwardAccel, NAME_None, true);
	}

	const float Dt = FMath::Clamp(DeltaSeconds, 1.0f / 240.0f, 0.1f);
	const bool bAllowTangentGuide = !bBidirectional || TangentSpeedCm >= TwoWayMinApproachSpeedCm;
	if (bAllowTangentGuide && ArcAssistMinTangentSpeedCm > 0.0f && TangentSpeedCm < ArcAssistMinTangentSpeedCm)
	{
		const float NeededAccel = (ArcAssistMinTangentSpeedCm - TangentSpeedCm) / Dt;
		const float GuideAccel = FMath::Clamp(NeededAccel, 0.0f, FMath::Max(ArcAssistMaxGuideAccelCm, 0.0f));
		if (GuideAccel > 0.0f)
		{
			BallSphere->AddForce(Tangent * GuideAccel, NAME_None, true);
		}
	}

	// Keep only the dangerous "leaving the surface" component under control. Tangent and inward motion
	// are untouched, so normal player movement remains available until the redirect takes over.
	if (ArcAssistMaxOutwardSpeedCm >= 0.0f)
	{
		Velocity = BallSphere->GetPhysicsLinearVelocity();
		const FVector OutwardDirection = -InwardDirection;
		const float OutwardSpeed = FVector::DotProduct(Velocity, OutwardDirection);
		if (OutwardSpeed > ArcAssistMaxOutwardSpeedCm)
		{
			Velocity -= OutwardDirection * (OutwardSpeed - ArcAssistMaxOutwardSpeedCm);
			BallSphere->SetPhysicsLinearVelocity(Velocity);
		}
	}

	if (bArcAssistDebugLog)
	{
		UE_LOG(LogTemp, VeryVerbose,
			TEXT("[GSArcAssist] %s hit=%s gap=%.1f speed=%.0f tangent=%.0f inwardAccel=%.0f loc=(%.0f,%.0f,%.0f)"),
			*GetNameSafe(this),
			*GetNameSafe(SurfaceHit.GetActor()),
			GapCm,
			Velocity.Size(),
			TangentSpeedCm,
			InwardAccel,
			BallSphere->GetComponentLocation().X,
			BallSphere->GetComponentLocation().Y,
			BallSphere->GetComponentLocation().Z);
	}
}

void AGSOneWayGravityRedirectVolume::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnabled || !TriggerBox)
	{
		ResetArcAssistState();
		return;
	}

	AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Ball)
	{
		ClearOwnedRedirect(false);
		ObservedBall.Reset();
		ResetArcAssistState();
		return;
	}

	if (ObservedBall.Get() != Ball)
	{
		ObservedBall = Ball;
		ClearOwnedRedirect(false);
		ResetArcAssistState();
	}

	USphereComponent* BallSphere = Ball->GetBallCollisionComponent();
	if (!BallSphere)
	{
		return;
	}

	const bool bInside = IsBallInsideBox(*TriggerBox, *BallSphere, bExpandTriggerByBallRadius, TriggerPaddingCm);
	if (!bInside)
	{
		// Leaving the redirect trigger rearms it for a future pass.
		bConsumedUntilExit = false;
	}

	// Manage/release only the redirect this volume successfully started.
	if (bOwnsRedirect)
	{
		ResetArcAssistState();

		AGSRollingBallPawn* OwnedBall = ActiveBall.Get();
		if (!OwnedBall)
		{
			ClearOwnedRedirect(true);
			return;
		}

		if (!OwnedBall->IsGravityRedirecting())
		{
			if (bDebugLog)
			{
				UE_LOG(LogTemp, Log,
					TEXT("[GSOneWayRedirect] %s redirect ended externally fire#%u t=%.3f"),
					*GetNameSafe(this), FireSerial,
					GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f);
			}
			ClearOwnedRedirect(true);
			return;
		}

		const float Progress = OwnedBall->GetGravityRedirectProgress();
		const FVector Target = bRuntimeEntryResolved
			? RuntimeExitGravity
			: GSGravity::DirectionToVector(TargetGravityDirection).GetSafeNormal();
		const FVector Current = OwnedBall->GetActiveGravityDirection().GetSafeNormal();
		const float TargetDot = (!Target.IsNearlyZero() && !Current.IsNearlyZero())
			? FVector::DotProduct(Current, Target)
			: -1.0f;

		if (Progress >= CompletionProgress && TargetDot >= CompletionTargetMinDot)
		{
			OwnedBall->EndGravityRedirect();
			if (bDebugLog)
			{
				UE_LOG(LogTemp, Log,
					TEXT("[GSOneWayRedirect] %s completed/released fire#%u t=%.3f progress=%.3f targetDot=%.4f inside=%d"),
					*GetNameSafe(this), FireSerial,
					GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f,
					Progress, TargetDot, bInside ? 1 : 0);
			}
			ClearOwnedRedirect(true);
		}
		return;
	}

	// v7 CONTACT-AUTHORITATIVE BEVEL:
	// In bidirectional bevel mode the REAL curved-surface contact is the trigger. Do not require the
	// ball center to enter TriggerBox first: a thin/misaligned trigger was able to miss the exact frame
	// where the sphere physically touched the bevel, producing the visible failure "hit arc, fall through".
	// TryBeginTwoWayFaceCapture is still fail-closed: it only succeeds when the probe actually hits one
	// of the explicitly tagged bevel actors, the surface gap is within TwoWayArcContactMarginCm, the
	// contact normal belongs to the A<->B quadrant, and motion is not clearly away from the exit.
	if (bBidirectional)
	{
		if (!Ball->IsFaceCapturing() && !Ball->IsFaceCaptureCoolingDown() && TryBeginTwoWayFaceCapture(*Ball))
		{
			return;
		}

		// FaceCapture is the ONLY force/gravity owner for this dedicated bevel. The legacy ArcAssist
		// path is deliberately disabled in bidirectional mode so it cannot compete with capture.
		ResetArcAssistState();
		return;
	}

	// Legacy one-way behavior keeps the original TriggerBox semantics and optional ArcAssist.
	if (bInside && !bConsumedUntilExit && TryBeginRedirect(*Ball))
	{
		return;
	}

	UpdateArcAssist(*Ball, DeltaSeconds);
}
