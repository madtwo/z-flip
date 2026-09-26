// GravityShift - one-way wall -> floor redirect volume with local convex-arc assist.

#include "GSOneWayGravityRedirectVolume.h"

#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "GSRollingBallPawn.h"

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
		UE_LOG(LogTemp, Log,
			TEXT("[GSOneWayRedirect] %s armed entry=%s target=%s center=%s extent=%s centerOnly=%d assistSurfaces=%d"),
			*GetNameSafe(this),
			*GSGravity::GetDirectionDisplayName(EntryGravityDirection),
			*GSGravity::GetDirectionDisplayName(TargetGravityDirection),
			*TriggerBox->GetComponentLocation().ToString(),
			*TriggerBox->GetScaledBoxExtent().ToString(),
			bExpandTriggerByBallRadius ? 0 : 1,
			ArcAssistSurfaces.Num());
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

bool AGSOneWayGravityRedirectVolume::TryBeginRedirect(AGSRollingBallPawn& Ball)
{
	if (Ball.IsGravityRedirecting() || Ball.IsFaceCapturing())
	{
		// Never steal the pawn from either redirect system.
		return false;
	}

	const FVector Entry = GSGravity::DirectionToVector(EntryGravityDirection).GetSafeNormal();
	const FVector Target = GSGravity::DirectionToVector(TargetGravityDirection).GetSafeNormal();
	const FVector Current = Ball.GetActiveGravityDirection().GetSafeNormal();

	if (Entry.IsNearlyZero() || Target.IsNearlyZero() || Current.IsNearlyZero())
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

	const float EntryDot = FVector::DotProduct(Current, Entry);
	const float TargetDot = FVector::DotProduct(Current, Target);
	if (TargetDot >= TargetAlreadyMinDot || EntryDot < EntryGravityMinDot)
	{
		return false;
	}

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
			TEXT("[GSOneWayRedirect] %s fired#%u(instant) t=%.3f current=(%.2f,%.2f,%.2f) entryDot=%.3f target=%s axis=(%.2f,%.2f,%.2f) ride=%.0f path=%.0f"),
			*GetNameSafe(this),
			FireSerial,
			GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0f,
			Current.X, Current.Y, Current.Z,
			EntryDot,
			*GSGravity::GetDirectionDisplayName(TargetGravityDirection),
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
	if (!GetWorld() || ArcAssistSurfaceNameContains.IsEmpty())
	{
		return;
	}

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Candidate = *It;
		if (Candidate && Candidate != this && Candidate->GetName().Contains(ArcAssistSurfaceNameContains))
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

	const FVector Axis = ArcAssistAxis.GetSafeNormal();
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

	const FVector RequiredGravity = GSGravity::DirectionToVector(ArcAssistGravityDirection).GetSafeNormal();
	const FVector CurrentGravity = Ball.GetActiveGravityDirection().GetSafeNormal();
	if (RequiredGravity.IsNearlyZero() || CurrentGravity.IsNearlyZero()
		|| FVector::DotProduct(CurrentGravity, RequiredGravity) < ArcAssistGravityMinDot)
	{
		ResetArcAssistState();
		return;
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

	const FVector Axis = ArcAssistAxis.GetSafeNormal();
	FVector Tangent = FVector::CrossProduct(Axis, InwardDirection).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		return;
	}

	FVector Velocity = BallSphere->GetPhysicsLinearVelocity();
	if (!LastArcAssistTangent.IsNearlyZero())
	{
		if (FVector::DotProduct(Tangent, LastArcAssistTangent) < 0.0f)
		{
			Tangent *= -1.0f;
		}
	}
	else
	{
		// On first contact, select the sign that best matches the existing motion. If nearly stationary,
		// prefer the upward tangent because the reported traversal starts by climbing +Z.
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
	if (ArcAssistMinTangentSpeedCm > 0.0f && TangentSpeedCm < ArcAssistMinTangentSpeedCm)
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
		const FVector Target = GSGravity::DirectionToVector(TargetGravityDirection).GetSafeNormal();
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

	// Redirect has priority over the lip assist. If this tick fires, TryBeginRedirect clears assist state
	// and the function returns immediately, so no assist force is applied after redirect ownership starts.
	if (bInside && !bConsumedUntilExit && TryBeginRedirect(*Ball))
	{
		return;
	}

	UpdateArcAssist(*Ball, DeltaSeconds);
}
