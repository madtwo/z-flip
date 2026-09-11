#include "GSRollingBallPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

#include "GSGravityBodyComponent.h"
#include "GSGravityManager.h"
#include "GSInteractable.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
#include "GSRailCameraComponent.h"
#include "GSResettableComponent.h"
#include "GSSurfaceReceiverComponent.h"
#include "GSWorldState.h"

AGSRollingBallPawn::AGSRollingBallPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	BallCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BallCollision"));
	BallCollision->SetSphereRadius(50.0f, false);
	BallCollision->SetSimulatePhysics(true);
	BallCollision->SetEnableGravity(false);
	BallCollision->SetCollisionProfileName(TEXT("PhysicsActor"));
	BallCollision->SetUseCCD(true);
	SetRootComponent(BallCollision);

	// Zero restitution so natural contact bounce is removed at the solver layer:
	// the engine default (PhysicalMaterial.cpp) is 0.3 and the ball ships no
	// surface override, so every contact micro-bounced at 0.3 regardless of the
	// landing-response code. Force the combine rule to Min so the ball never
	// restitutes against any world surface (0 + anything => 0); all bounce and
	// anti-gravity launch is applied by code (LandingResponse) instead.
	{
		UPhysicalMaterial* PhysMat = NewObject<UPhysicalMaterial>(GetTransientPackage(), TEXT("GSBallZeroRestitution"));
		PhysMat->Restitution = 0.0f;
		PhysMat->bOverrideRestitutionCombineMode = true;
		PhysMat->RestitutionCombineMode = EFrictionCombineMode::Min;
		BallCollision->SetPhysMaterialOverride(PhysMat);
	}

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetupAttachment(BallCollision);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetEnableGravity(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		BallMesh->SetStaticMesh(SphereAsset.Object);
	}

	// Default tuning DataAsset (created by generate_data_assets.py). The game mode
	// spawns the pawn natively, so without this the ball runs on header defaults
	// (landing reverse at 900 instead of the profile's 1400 — every shed-height
	// landing flipped gravity).
	static ConstructorHelpers::FObjectFinder<UGSBallProfile> BallProfileAsset(TEXT("/Game/GravityShift/Data/Profiles/DA_GS_Ball_Default"));
	if (BallProfileAsset.Succeeded())
	{
		BallProfile = BallProfileAsset.Object;
	}

	CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
	CameraPivot->SetupAttachment(BallCollision);
	// UpdateCamera writes an absolute world rotation every tick; treat it as
	// absolute so the rolling ball's rotation never bleeds into the camera rig.
	CameraPivot->SetUsingAbsoluteRotation(true);
	// Location: BeginPlay applies bUseAbsoluteCameraLocation. Default ON - a
	// relative pivot inherits the physics ball's per-frame displacement between
	// pawn ticks, which bypasses the rail camera's exponential smoothing entirely
	// and shows up as small instant jitter while moving.

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(CameraPivot);
	CameraArm->bUsePawnControlRotation = false;
	// The gravity flip is a pitch rotation on CameraPivot; the arm must pass
	// pivot rotation through to the camera or the flip never reaches the view.
	CameraArm->bInheritPitch = true;
	CameraArm->bInheritYaw = true;
	CameraArm->bInheritRoll = true;
	CameraArm->TargetArmLength = CameraArmLengthCm;
	CameraArm->SetUsingAbsoluteRotation(false);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	Camera->bUsePawnControlRotation = false;

	GravityBody = CreateDefaultSubobject<UGSGravityBodyComponent>(TEXT("GravityBody"));
	SurfaceReceiver = CreateDefaultSubobject<UGSSurfaceReceiverComponent>(TEXT("SurfaceReceiver"));
	LandingResponse = CreateDefaultSubobject<UGSLandingResponseComponent>(TEXT("LandingResponse"));
	Resettable = CreateDefaultSubobject<UGSResettableComponent>(TEXT("Resettable"));
	RailCamera = CreateDefaultSubobject<UGSRailCameraComponent>(TEXT("RailCamera"));

	LandingResponseRef = LandingResponse;
}

void AGSRollingBallPawn::BeginPlay()
{
	Super::BeginPlay();

	RefreshSystemReferences();

	if (CameraPivot)
	{
		// Apply the instance-settable absolute-location switch here so PIE tests
		// can flip it for A/B jitter comparisons.
		CameraPivot->SetUsingAbsoluteLocation(bUseAbsoluteCameraLocation);
	}

	if (BallProfile)
	{
		ApplyBallProfile(BallProfile);
	}

	CameraArm->TargetArmLength = CameraArmLengthCm;
	// 相机臂长由 UpdateCamera 的自建探针+平滑接管:弹簧臂自带探针在台阶/平台边缘会
	// 700↔175 逐帧翻转,相机被前后拽动 ±500cm(爬楼梯画面抖动)。自带探针与滞后都关。
	CameraArm->bDoCollisionTest = false;
	CameraArm->bEnableCameraLag = false;

	TargetCameraUp = bCameraFlipsWithGravity ? -GetActiveGravityDirection() : FVector::UpVector;
	CurrentCameraUp = TargetCameraUp;
	// 兼容旧序列化实例的 CameraYawDegrees:开局把航向从旧偏航角换算出来,此后航向
	// 是唯一朝向状态(旧属性不再参与计算)。
	CameraAimHeading = FQuat(FVector::UpVector, FMath::DegreesToRadians(CameraYawDegrees))
		.RotateVector(FVector::ForwardVector);
	CurrentCameraRotation = BuildCameraRotation(TargetCameraUp);
	bCameraRotationReady = true;

	if (bAutoPossessFirstPlayer)
	{
		AutoPossessPlayer = EAutoReceiveInput::Player0;
	}

	if (Resettable)
	{
		Resettable->CaptureInitialState();
	}
}

void AGSRollingBallPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Input is polled in Tick: BindAction on a level-placed instance loses its InputComponent
	// in this project (documented pitfall). Polling IsInputKeyDown is immune to input-stack timing.
}

void AGSRollingBallPawn::RefreshSystemReferences()
{
	if (!GravityManager)
	{
		GravityManager = AGSGravityManager::FindGravityManager(this);
	}

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
		if (WorldStateManager)
		{
			WorldStateManager->RegisterPlayer(this);
		}
	}

	if (GravityManager)
	{
		GravityManager->OnGravityChanged.RemoveDynamic(this, &AGSRollingBallPawn::HandleGravityChanged);
		GravityManager->OnGravityChanged.AddDynamic(this, &AGSRollingBallPawn::HandleGravityChanged);
	}

	if (GravityBody)
	{
		GravityBody->RefreshReferences();
	}

	if (LandingResponse)
	{
		LandingResponse->RefreshReferences();
	}
}

void AGSRollingBallPawn::ApplyBallProfile(UGSBallProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	BallProfile = NewProfile;

	if (BallCollision)
	{
		BallCollision->SetSphereRadius(NewProfile->RadiusCm, false);
		BallCollision->SetMassOverrideInKg(NAME_None, NewProfile->MassKg, true);
		BallCollision->SetLinearDamping(NewProfile->LinearDamping);
		BallCollision->SetAngularDamping(NewProfile->AngularDamping);
	}

	if (BallMesh)
	{
		// /Engine/BasicShapes/Sphere has a 50cm radius.
		const float Scale = NewProfile->RadiusCm / 50.0f;
		BallMesh->SetWorldScale3D(FVector(Scale));
	}

	RollTorqueAcceleration = NewProfile->RollTorqueAcceleration;
	StopTorqueAcceleration = NewProfile->StopTorqueAcceleration;
	DriveAccelerationCm = NewProfile->DriveAccelerationCm;
	ReleaseBrakeHz = NewProfile->ReleaseBrakeHz;
	MaximumPlanarSpeedCm = NewProfile->MaximumPlanarSpeedCm;
	bClampPlanarSpeed = NewProfile->bClampPlanarSpeed;
	bAllowAirControl = NewProfile->bAllowAirControl;
	AirControlAccelerationCm = NewProfile->AirControlAccelerationCm;

	CameraFlipDurationSeconds = NewProfile->CameraFlipDurationSeconds;
	CameraFollowInterpSpeed = NewProfile->CameraFollowInterpSpeed;
	CameraArmLengthCm = NewProfile->CameraArmLengthCm;
	CameraYawDegreesPerMouseUnit = NewProfile->CameraYawDegreesPerMouseUnit;
	CameraPitchDegreesPerMouseUnit = NewProfile->CameraPitchDegreesPerMouseUnit;
	MaximumCameraPitchDegrees = NewProfile->MaximumCameraPitchDegrees;
	bCameraFlipsWithGravity = NewProfile->bCameraFlipsWithGravity;

	ManualVelocityRetention = NewProfile->ManualVelocityRetention;
	AutomaticVelocityRetention = NewProfile->AutomaticVelocityRetention;
	AutomaticMaxCarrySpeedCm = NewProfile->AutomaticMaxCarrySpeedCm;
	InteractionRadiusCm = NewProfile->InteractionRadiusCm;

	if (GravityBody)
	{
		GravityBody->GravityScale = NewProfile->GravityScale;
		GravityBody->GravityAxisDragHz = NewProfile->GravityAxisDragHz;
		GravityBody->TangentDragHz = NewProfile->TangentDragHz;
		GravityBody->MaximumSpeedCm = NewProfile->MaximumPlanarSpeedCm * 4.0f;
	}

	if (LandingResponse)
	{
		LandingResponse->AutoReverseMode = NewProfile->AutoReverseMode;
		LandingResponse->AutoReverseFallSpeedCm = NewProfile->AutoReverseFallSpeedCm;
		LandingResponse->AutoReverseFallDistanceCm = NewProfile->AutoReverseFallDistanceCm;
		LandingResponse->LandingAutoReverseAtSpeedCm = NewProfile->LandingAutoReverseAtSpeedCm;
		LandingResponse->BounceSpeedCm = NewProfile->BounceSpeedCm;
		LandingResponse->NoResponseBelowImpactSpeedCm = NewProfile->NoResponseBelowImpactSpeedCm;
		LandingResponse->AutomaticVelocityRetention = NewProfile->AutomaticVelocityRetention;
		LandingResponse->AutomaticMaxCarrySpeedCm = NewProfile->AutomaticMaxCarrySpeedCm;
	}
}

FVector AGSRollingBallPawn::GetActiveGravityDirection() const
{
	// 转向器过渡期间以过渡中的中间方向为准:相机抬升、驱动平面、物理重力读到同一个源,
	// 滑行全程连续;过渡结束提交管理器后两者一致,不会二次跳变。
	if (bGravityRedirectActive)
	{
		return GravityRedirectCurrent;
	}
	return GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
}

float AGSRollingBallPawn::GetGravityRedirectProgress() const
{
	if (!bGravityRedirectActive)
	{
		return 1.0f;
	}
	return FMath::Clamp(GravityRedirectPathCm / FMath::Max(GravityRedirectPathLength, 1.0f), 0.0f, 1.0f);
}

void AGSRollingBallPawn::BeginGravityRedirect(FVector TargetGravityDirection, float RideSpeedCm, FVector BendAxis, float RidePathLengthCm)
{
	const FVector Target = TargetGravityDirection.GetSafeNormal();
	if (Target.IsNearlyZero())
	{
		return;
	}
	const FVector Current = GetActiveGravityDirection().GetSafeNormal();
	if (FVector::DotProduct(Current, Target) > 0.9995f)
	{
		// 已经在目标面上(例如刚滑出又路过触发盒),无需过渡。
		return;
	}

	GravityRedirectFrom = Current;
	GravityRedirectTo = Target;
	GravityRedirectCurrent = Current;
	GravityRedirectAxis = BendAxis.GetSafeNormal();
	if (GravityRedirectAxis.IsNearlyZero())
	{
		GravityRedirectAxis = FVector::CrossProduct(Current, Target).GetSafeNormal();
	}
	GravityRedirectSpeed = FMath::Max(RideSpeedCm, 0.0f);
	GravityRedirectPathLength = FMath::Max(RidePathLengthCm, 1.0f);
	GravityRedirectPathCm = 0.0f;
	GravityRedirectHoldElapsed = 0.0f;
	bGravityRedirectRotationCommitted = false;
	bGravityRedirectActive = true;

	if (GravityBody)
	{
		GravityBody->SetGravityDirectionOverride(GravityRedirectCurrent);
	}
}

void AGSRollingBallPawn::EndGravityRedirect()
{
	if (!bGravityRedirectActive)
	{
		return;
	}

	// 旋转没走完就补完:中途释放不能留下一个介于两个面之间的重力方向。
	GravityRedirectCurrent = GravityRedirectTo;
	if (!bGravityRedirectRotationCommitted)
	{
		bGravityRedirectRotationCommitted = true;
		if (GravityManager)
		{
			GravityManager->RequestGravityDirection(
				GSGravity::VectorToDirection(GravityRedirectTo), this, EGSGravityChangeReason::SCRIPTED, true);
		}
	}
	if (GravityBody)
	{
		GravityBody->SetGravityDirectionOverride(FVector::ZeroVector);
	}
	bGravityRedirectActive = false;
}

void AGSRollingBallPawn::UpdateGravityRedirect(float DeltaSeconds)
{
	if (!bGravityRedirectActive)
	{
		return;
	}

	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
	GravityRedirectHoldElapsed += Dt;

	// 1) 滑行:指令速度 = 弯道当前切向 forward = cross(当前上, 弯道轴),随重力一起旋转。
	//    再用"球下探针"找到实际接触面:切向速度投影到接触面切平面,并按空隙补一个
	//    法向吸附速度——理想弧线与布尔网格实际形状的偏差都被这一步吃掉,球贴着滑梯
	//    面走,不会在半坡飞出去。
	const FVector Up = (-GravityRedirectCurrent).GetSafeNormal();
	FVector Forward = FVector::CrossProduct(Up, GravityRedirectAxis).GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		Forward = GravityRedirectTo;
	}

	const float Radius = BallCollision ? BallCollision->GetScaledSphereRadius() : 0.0f;
	float GapCm = 0.0f;
	FVector ContactNormal = FVector::ZeroVector;
	bool bHasSurface = false;
	FString ContactName = TEXT("air");
	if (const UWorld* World = GetWorld())
	{
		if (BallCollision)
		{
			const FVector BallLoc = BallCollision->GetComponentLocation();
			FHitResult GroundHit;
			FCollisionQueryParams GroundParams(SCENE_QUERY_STAT(GSRedirectProbe), false, this);
			if (World->LineTraceSingleByChannel(GroundHit, BallLoc,
				BallLoc - Up * (Radius + SurfaceFollowRangeCm), ECC_WorldStatic, GroundParams))
			{
				bHasSurface = true;
				ContactNormal = GroundHit.Normal.GetSafeNormal();
				GapCm = GroundHit.Distance - Radius;
				if (GroundHit.GetActor())
				{
					ContactName = GroundHit.GetActor()->GetName();
				}
			}
		}
	}

	FVector DesiredVelocity = Forward * GravityRedirectSpeed;
	if (bHasSurface && !ContactNormal.IsNearlyZero())
	{
		const float NormalSpeed = FMath::Clamp(-GapCm * SurfaceFollowGain,
			-GravityRedirectSpeed, GravityRedirectSpeed);
		DesiredVelocity = FVector::VectorPlaneProject(DesiredVelocity, ContactNormal)
			+ ContactNormal * NormalSpeed;
	}

	const FVector ActualVelocity = BallCollision ? BallCollision->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	if (BallCollision && BallCollision->IsSimulatingPhysics() && GravityRedirectSpeed > 0.0f)
	{
		BallCollision->SetPhysicsLinearVelocity(DesiredVelocity.GetSafeNormal() * GravityRedirectSpeed);
		// 旋转进度按"实际走掉的位移"累计(球被挡住时重力先不转,不会脱节)。
		GravityRedirectPathCm += ActualVelocity.Size() * Dt;
	}

	// 2) 重力随滑行距离旋转(smoothstep 起步/收尾柔和)。按距离而不是按时间推进:
	//    球滚得快就转得快,离开坡顶时重力已到出口方向,不会半路"重力转完球还在坡上"。
	const float Alpha = FMath::Clamp(GravityRedirectPathCm / GravityRedirectPathLength, 0.0f, 1.0f);
	const float Eased = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	const FQuat Delta = FQuat::FindBetweenNormals(GravityRedirectFrom, GravityRedirectTo);
	GravityRedirectCurrent = FQuat::Slerp(FQuat::Identity, Delta, Eased).RotateVector(GravityRedirectFrom);

	if (GravityBody)
	{
		GravityBody->SetGravityDirectionOverride(GravityRedirectCurrent);
	}

	if (bRedirectDebugLog)
	{
		const FVector BallLoc = BallCollision ? BallCollision->GetComponentLocation() : FVector::ZeroVector;
		UE_LOG(LogTemp, Log, TEXT("[GSRedirect] t=%.2f ball=(%.0f,%.0f,%.0f) actual=(%.0f,%.0f,%.0f) cmd=(%.0f,%.0f,%.0f) contact=%s gap=%.0f up=(%.2f,%.2f,%.2f) prog=%.2f/%.0f"),
			GravityRedirectHoldElapsed, BallLoc.X, BallLoc.Y, BallLoc.Z,
			ActualVelocity.X, ActualVelocity.Y, ActualVelocity.Z,
			DesiredVelocity.X, DesiredVelocity.Y, DesiredVelocity.Z,
			*ContactName, GapCm, Up.X, Up.Y, Up.Z, GravityRedirectPathCm, GravityRedirectPathLength);
	}

	// 3) 旋转走完即提交(此时物理方向与管理器方向一致,提交无跳变);滑行本身持续到
	//    球离开滑梯——由转向器调 EndGravityRedirect。
	if (!bGravityRedirectRotationCommitted && Alpha >= 1.0f)
	{
		bGravityRedirectRotationCommitted = true;
		if (GravityManager)
		{
			GravityManager->RequestGravityDirection(
				GSGravity::VectorToDirection(GravityRedirectTo), this, EGSGravityChangeReason::SCRIPTED, true);
		}
	}

	// 4) 保险:超时强制释放,避免异常状态下永久夺走玩家控制。
	if (GravityRedirectHoldElapsed >= GravityRedirectMaxSeconds)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GSRedirect] 滑行超时 %.1fs 强制释放(检查转向器触发盒/出口方向)"),
			GravityRedirectHoldElapsed);
		EndGravityRedirect();
	}
}

FQuat AGSRollingBallPawn::BuildCameraRotation(const FVector& UpVector, float AdditionalPitchDegrees) const
{
	FVector Up = UpVector.GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		Up = FVector::UpVector;
	}

	// 世界航向投影到目标上轴平面:±Z 翻转(G)时航向保持原世界方向,视角翻转后
	// 仍对准同一个方向(旧实现把偏航角绕"新上轴"重算,+Z/-Z 符号互换导致瞄准方向
	// 镜像,且 FindBetweenNormals 在反向平行时旋转轴不确定)。
	FVector Forward = CameraAimHeading - Up * FVector::DotProduct(CameraAimHeading, Up);
	Forward = Forward.GetSafeNormal();
	if (Forward.IsNearlyZero())
	{
		// 航向几乎平行于上轴(比如墙态重力):退化到世界前向的投影。
		Forward = FVector::ForwardVector - Up * FVector::DotProduct(FVector::ForwardVector, Up);
		Forward = Forward.GetSafeNormal();
	}
	const FVector Right = FVector::CrossProduct(Up, Forward).GetSafeNormal();

	const FQuat Base = FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat();
	const float TotalPitch = FMath::Clamp(CameraPitchDegrees + AdditionalPitchDegrees,
		-MaximumCameraPitchDegrees, MaximumCameraPitchDegrees);
	const FQuat PitchQuat = FQuat(Right, FMath::DegreesToRadians(-TotalPitch));
	return PitchQuat * Base;
}

void AGSRollingBallPawn::UpdateCamera(float DeltaSeconds)
{
	if (!CameraPivot)
	{
		return;
	}

	// 平滑/积分一律钳位 dt(PIE 暂停恢复/掉帧会塞进异常甚至负 dt,指数平滑会外推)。
	DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);

	// Rail camera: the position rides the level's camera rail and the view is
	// rebuilt around world up with a small clamped gimbal. The chase rig below
	// stays as the fallback for levels without a rail.
	FVector RailPosition = FVector::ZeroVector;
	FQuat RailRotation = FQuat::Identity;
	if (RailCamera && RailCamera->ComputeCameraPose(RailPosition, RailRotation, DeltaSeconds))
	{
		if (!bRailCamActive)
		{
			bRailCamActive = true;
			CameraArm->bDoCollisionTest = false;
			CameraArm->TargetArmLength = 0.0f;
		}
		if (bRailCamDebugLog && CameraPivot)
		{
			// pivotPrev = where last tick's write has drifted to by now: with a
			// relative-located pivot this includes the ball's intra-frame motion
			// (the unsmoothed leak); with absolute location it stays at the write.
			const FVector PrevPivot = CameraPivot->GetComponentLocation();
			const FVector BallLoc = BallCollision ? BallCollision->GetComponentLocation() : GetActorLocation();
			UE_LOG(LogTemp, Log, TEXT("[RailCam] t=%06.2f dt=%.4f pivotPrev=(%.1f,%.1f,%.1f) ball=(%.1f,%.1f,%.1f) railTarget=(%.1f,%.1f,%.1f)"),
				GetWorld()->GetTimeSeconds(), DeltaSeconds,
				PrevPivot.X, PrevPivot.Y, PrevPivot.Z,
				BallLoc.X, BallLoc.Y, BallLoc.Z,
				RailPosition.X, RailPosition.Y, RailPosition.Z);
		}
		CameraPivot->SetWorldLocationAndRotation(RailPosition, RailRotation);
		CurrentCameraUp = RailRotation.RotateVector(FVector::UpVector);
		return;
	}

	if (bRailCamActive)
	{
		bRailCamActive = false;
		// 回退相机的地形适配由下面自建探针负责(自带探针在台阶边缘逐帧翻转,已关)。
		CameraArm->bDoCollisionTest = false;
		CameraArm->TargetArmLength = CameraArmLengthCm;
	}

	TargetCameraUp = bCameraFlipsWithGravity ? -GetActiveGravityDirection() : FVector::UpVector;
	const FQuat Target = BuildCameraRotation(TargetCameraUp);

	if (!bCameraRotationReady)
	{
		CurrentCameraRotation = Target;
		bCameraRotationReady = true;
	}
	else
	{
		// Quaternion slerp: safe across a 180 degree flip (never Euler interpolation).
		// 时按角距自适应:大角度翻转保留防晕慢速;鼠标微调近乎即时(旧实现统一
		// CameraFlipDurationSeconds,鼠标转向拖 0.35s 的"严重惯性")。
		const float AngleDeg = FMath::RadiansToDegrees(CurrentCameraRotation.AngularDistance(Target));
		const float Tau = CameraFlipDurationSeconds
			* FMath::Clamp(AngleDeg / 90.0f, 0.12f, 1.0f);
		const float Alpha = 1.0f - FMath::Exp(-DeltaSeconds / FMath::Max(Tau, 0.001f));
		CurrentCameraRotation = FQuat::Slerp(CurrentCameraRotation, Target, Alpha);
	}

	// 球放画面下三分之一:枢轴沿"支撑面上侧"(=−重力)抬升,随重力翻转平滑摆动
	// (地板态相机在球上方,天花板态在球下方的房间内侧——两侧都不颠倒画面)。
	// 换边摆速 150°/s(≈1.2s 走完 180°,与视角翻转同步):摆快了枢轴会被拽着在
	// 0.45s 内掠过 300cm,就是按下 G 时那记"相机猛地向下/上窜"的抖动来源。
	const FVector SupportUp = (-GetActiveGravityDirection()).GetSafeNormal();
	CameraLiftDirection = FMath::VInterpNormalRotationTo(CameraLiftDirection, SupportUp, DeltaSeconds, 150.0f);
	const FVector BallLoc = BallCollision ? BallCollision->GetComponentLocation() : GetActorLocation();
	// 碰撞探针把臂压短时,抬升等比缩短 → 球的角取景恒定(贴墙不出房、球不出框)。
	// —— 相机臂长:自建探针 + 去弹平滑(替掉弹簧臂自带探针的逐帧翻转)——
	// 自带探针在台阶/平台边缘会 700↔175 逐帧翻转,相机被前后拽动 ±500cm(爬楼梯画面
	// 抖动主因)。改为:沿"枢轴→期望相机位"打 ECC_Camera 探针;命中立即收短(防穿墙),
	// 只有连续 0.25s 无命中才缓慢放长——翻转被去弹,相机稳定不再弹跳。
	const FVector PivotLoc = CameraPivot->GetComponentLocation();
	const FVector ArmDir = -CurrentCameraRotation.GetForwardVector();
	const FVector DesiredCamPos = PivotLoc + ArmDir * CameraArmLengthCm;
	float SafeArmCm = CameraArmLengthCm;
	FString ArmHitName = TEXT("none");
	{
		FHitResult ArmHit;
		FCollisionQueryParams ArmParams(SCENE_QUERY_STAT(GSFallbackCamProbe), false, this);
		if (GetWorld()->LineTraceSingleByChannel(ArmHit, PivotLoc, DesiredCamPos, ECC_Camera, ArmParams))
		{
			SafeArmCm = FMath::Max((ArmHit.Location - PivotLoc).Size() - 20.0f, 40.0f);
			if (ArmHit.GetActor())
			{
				ArmHitName = ArmHit.GetActor()->GetName();
			}
		}
	}
	if (SmoothedArmLengthCm < 0.0f)
	{
		SmoothedArmLengthCm = SafeArmCm;
	}
	if (SafeArmCm < CameraArmLengthCm - 1.0f)
	{
		ProbeClearSeconds = 0.0f;
		SmoothedArmLengthCm = FMath::FInterpTo(SmoothedArmLengthCm, SafeArmCm, DeltaSeconds, 8.0f);
	}
	else
	{
		ProbeClearSeconds += DeltaSeconds;
		if (ProbeClearSeconds > ArmExtendHoldSeconds)
		{
			SmoothedArmLengthCm = FMath::FInterpTo(SmoothedArmLengthCm, CameraArmLengthCm, DeltaSeconds,
				FMath::Max(ArmLengthInterpSpeed, 0.1f));
		}
	}
	CameraArm->TargetArmLength = SmoothedArmLengthCm;
	const float LiftScale = FMath::Clamp(SmoothedArmLengthCm / FMath::Max(CameraArmLengthCm, 1.0f), 0.0f, 1.0f);
	const FVector TargetPivot = BallLoc + CameraLiftDirection * CameraPivotLiftHeightCm * LiftScale;
	// 位置指数平滑:G 的平移过渡/翻滚带抖动都被滤掉(导轨相机同款手法)。
	if (!bPivotSmoothed)
	{
		SmoothedPivotLocation = TargetPivot;
		bPivotSmoothed = true;
	}
	else
	{
		const float PosAlpha = 1.0f - FMath::Exp(-FMath::Max(CameraFollowInterpSpeed, 0.1f) * DeltaSeconds);
		SmoothedPivotLocation = FMath::Lerp(SmoothedPivotLocation, TargetPivot, PosAlpha);
	}
	const FVector Location = SmoothedPivotLocation;
	CameraPivot->SetWorldLocationAndRotation(Location, CurrentCameraRotation);
	CurrentCameraUp = CurrentCameraRotation.RotateVector(FVector::UpVector);

	// 角度微调:轴向瞄准"球 + 抬升方向上的 AimUp"——把球压回画面下/上三分之一。
	// (地板态微俯 −1°,天花板态微抬 +17°)。俯仰必须从相机实际位置(枢轴沿水平
	// 航向后退一个臂长)起算,从枢轴起算会把瞄准点投影成垂直方向(俯仰打满钳位)。
	// (判别项与瞄准点均改用平滑后的抬升方向,理由见下方。) 
	// 角度微调:轴向瞄准"球 + 抬升方向上的 AimUp"——把球压回画面下/上三分之一。
	// ⚠ 必须用平滑后的 CameraLiftDirection,不能用瞬间换边的 SupportUp:否则 G 按下
	// 那一帧瞄准点瞬移 ~284cm,画面俯仰瞬间跳 ~22°(用户反馈的"突变"),随后又随枢轴
	// 慢慢飘回来。改用抬升方向后瞄准点与枢轴同步沿弧线连续移动,俯仰全程只变 ~1°。
	// 判别项同样用 dot(抬升方向,世界上):收敛后与 dot(支撑上,世界上) 等价,过程中连续。
	// 瞄准高度随抬升缩放同比收缩(与枢轴抬升同源):臂被压短时构图不变,俯仰不甩。
	const float AimUp = (CameraAimUpBaseCm
		+ CameraAimUpSwingCm * FVector::DotProduct(CameraLiftDirection, FVector::UpVector)) * LiftScale;
	const FVector AimTarget = BallLoc + CameraLiftDirection * AimUp;
	FVector HeadingHoriz = CameraAimHeading - TargetCameraUp * FVector::DotProduct(CameraAimHeading, TargetCameraUp);
	HeadingHoriz = HeadingHoriz.GetSafeNormal();
	if (HeadingHoriz.IsNearlyZero())
	{
		HeadingHoriz = FVector::ForwardVector - TargetCameraUp * FVector::DotProduct(FVector::ForwardVector, TargetCameraUp);
		HeadingHoriz = HeadingHoriz.GetSafeNormal();
	}
	// 用平滑后的实际臂长(不是配置的 700):臂长塌缩时瞄准点仍落在同一世界位置,
	// 画面俯仰不会随探针收短而摆动(爬楼梯时相机贴近,旧写法会甩一下)。
	const FVector CamPosApprox = Location - HeadingHoriz * SmoothedArmLengthCm;
	const FVector LookDir = (AimTarget - CamPosApprox).GetSafeNormal();
	const float AimPitchDeg = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp(FVector::DotProduct(LookDir, TargetCameraUp), -1.0f, 1.0f)));

	if (bFallbackCamDebugLog)
	{
		// 逐帧落盘(爬楼梯抖动诊断):ballZ=物理输入,pivot=平滑后,target=平滑前,
		// cam=弹簧臂实测相机位(含探针压臂),arm/scale=探针压臂与抬升缩放,pitch=瞄准俯仰。
		const FVector CamNow = CameraArm->GetSocketLocation(NAME_None);
		UE_LOG(LogTemp, Log, TEXT("[FallbackCam] t=%06.2f dt=%.4f ballZ=%.1f pivot=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f,%.1f) arm=%.1f raw=%.1f hit=%s scale=%.2f pitch=%.2f"),
			GetWorld()->GetTimeSeconds(), DeltaSeconds,
			BallLoc.Z,
			Location.X, Location.Y, Location.Z,
			TargetPivot.X, TargetPivot.Y, TargetPivot.Z,
			CamNow.X, CamNow.Y, CamNow.Z,
			SmoothedArmLengthCm, SafeArmCm, *ArmHitName, LiftScale, AimPitchDeg);
	}
	CameraPivot->SetWorldLocationAndRotation(
		Location, BuildCameraRotation(TargetCameraUp, AimPitchDeg));
	CurrentCameraUp = CurrentCameraRotation.RotateVector(FVector::UpVector);
}

void AGSRollingBallPawn::ApplyMovement(float DeltaSeconds)
{
	if (!BallCollision || !BallCollision->IsSimulatingPhysics())
	{
		return;
	}

	if (bGravityRedirectActive)
	{
		// 转向器滑行期间由重力旋转接管:不施加 WASD 驱动、不刹车,玩家输入不干扰滑行。
		return;
	}

	if (MoveInput.IsNearlyZero())
	{
		// No input: counter-torque brake so the ball stops quickly instead of
		// coasting on rolling friction (supported only; air keeps momentum).
		// 纯反力矩受低摩擦牵引限制(球会空转而线速度停不下来),叠加平面速度
		// 直接衰减:松键 ~1s 内停稳(重力竖直分量原样保留,不影响掉落)。
		if (LandingResponse && LandingResponse->IsSupported())
		{
			const FVector BrakeUp = -GetActiveGravityDirection();
			const FVector Velocity = BallCollision->GetPhysicsLinearVelocity();
			const float NormalSpeed = FVector::DotProduct(Velocity, BrakeUp);
			FVector Planar = Velocity - BrakeUp * NormalSpeed;
			Planar *= FMath::Exp(-ReleaseBrakeHz * DeltaSeconds);
			BallCollision->SetPhysicsLinearVelocity(BrakeUp * NormalSpeed + Planar);

			const float PlanarSpeed = Planar.Size();
			if (PlanarSpeed > 10.0f)
			{
				const FVector TorqueAxis = FVector::CrossProduct(BrakeUp, -Planar / PlanarSpeed);
				BallCollision->AddTorqueInRadians(TorqueAxis * StopTorqueAcceleration, NAME_None, true);
			}
		}
		return;
	}

	const FVector Up = -GetActiveGravityDirection();
	const FVector GravityDir = -Up;

	// Camera-relative movement basis: project the camera's OWN forward/right onto
	// the support plane. Deriving Right as Up x Forward flips it whenever Up flips,
	// while the rail camera keeps its world-up roll — A/D ended up mirrored on the
	// ceiling. The camera's right vector matches the screen on every surface.
	FVector Forward = CameraPivot ? CameraPivot->GetForwardVector() : GetActorForwardVector();
	FVector Right = CameraPivot ? CameraPivot->GetRightVector() : FVector::CrossProduct(Up, Forward);

	if (CameraPivot && FMath::Abs(Up.Z) < 0.5f)
	{
		// Wall: the camera's right is perpendicular to the wall, so the generic
		// projection collapses. Control spec: W/S roll horizontally along the
		// wall; A/D climb/descend — A climbs on the screen-left wall, D climbs on
		// the screen-right wall (gravity toward the camera's right = right wall).
		FVector Horizontal = FVector(Forward.X, Forward.Y, 0.0f);
		if (Horizontal.Normalize())
		{
			Forward = Horizontal;
			const float SideSign = FVector::DotProduct(GravityDir, CameraPivot->GetRightVector()) >= 0.0f ? 1.0f : -1.0f;
			Right = FVector(0.0, 0.0, SideSign);
		}
		// Degenerate (camera faces straight into the wall plane): keep the
		// generic projected basis computed above.
	}

	Forward = Forward - Up * FVector::DotProduct(Forward, Up);
	if (!Forward.Normalize())
	{
		Forward = GetActorForwardVector();
		Right = FVector::CrossProduct(Up, Forward);
	}
	Right = Right - Up * FVector::DotProduct(Right, Up);
	if (!Right.Normalize())
	{
		Right = FVector::CrossProduct(Up, Forward);
	}
	FVector Desired = Forward * MoveInput.Y + Right * MoveInput.X;
	if (!Desired.Normalize())
	{
		return;
	}

	const bool bSupported = LandingResponse ? LandingResponse->IsSupported() : false;

	if (bSupported)
	{
		// Drive: direct planar acceleration (mass-independent). 旧的力矩驱动在低摩擦
		// 接触下大量打滑(实测 ω·r≈3000 而 |v|≈240),滚动力矩转化不成位移,终端速度
		// 被切向拖拽死锁在 ~335。平面加速度绕开牵引耦合,终端 = DriveAccel/切向拖拽,
		// 只作用于 WASD 路径;重力/掉落完全不走这里。
		BallCollision->AddForce(Desired * DriveAccelerationCm, NAME_None, true);
	}
	else if (bAllowAirControl)
	{
		BallCollision->AddForce(Desired * AirControlAccelerationCm, NAME_None, true);
	}

	if (bClampPlanarSpeed && MaximumPlanarSpeedCm > 0.0f)
	{
		FVector Velocity = BallCollision->GetPhysicsLinearVelocity();
		const float NormalSpeed = FVector::DotProduct(Velocity, Up);
		FVector Planar = Velocity - Up * NormalSpeed;
		const float PlanarSpeed = Planar.Size();
		if (PlanarSpeed > MaximumPlanarSpeedCm)
		{
			Planar = Planar.GetSafeNormal() * MaximumPlanarSpeedCm;
			BallCollision->SetPhysicsLinearVelocity(Planar + Up * NormalSpeed);
		}
	}
}

void AGSRollingBallPawn::PollNativeInput()
{
	if (!bEnableNativePollingInput)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	// Rail mode owns the camera; accumulated mouse offsets only apply to the
	// chase rig, otherwise they would suddenly apply on the next rail handoff.
	if (!RailCamera || !RailCamera->IsDriving())
	{
		if (!FMath::IsNearlyZero(MouseX) || !FMath::IsNearlyZero(MouseY))
		{
			AddCameraLookInput(MouseX * CameraYawDegreesPerMouseUnit, -MouseY * CameraPitchDegreesPerMouseUnit);
		}
	}

	const float AxisX = (PC->IsInputKeyDown(RightKey) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(LeftKey) ? 1.0f : 0.0f);
	const float AxisY = (PC->IsInputKeyDown(ForwardKey) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(BackwardKey) ? 1.0f : 0.0f);
	SetMoveInput(FVector2D(AxisX, AxisY));

	// Debug: hold forward drive every tick (stair-climb camera shake reproduction).
	if (bDebugAutoDriveForward)
	{
		SetMoveInput(FVector2D(0.0f, 1.0f));
	}

	const bool bFlipDown = PC->IsInputKeyDown(FlipGravityKey);
	if (bFlipDown && !bFlipKeyWasDown)
	{
		HandleFlipPressed();
	}
	bFlipKeyWasDown = bFlipDown;

	const bool bInteractDown = PC->IsInputKeyDown(InteractKey);
	if (bInteractDown && !bInteractKeyWasDown)
	{
		TryInteract();
	}
	bInteractKeyWasDown = bInteractDown;

	const bool bResetDown = PC->IsInputKeyDown(ResetKey);
	if (bResetDown && !bResetKeyWasDown)
	{
		ResetToCheckpoint();
	}
	bResetKeyWasDown = bResetDown;

	// Player camera-distance keys: each press steps the rail camera's trail.
	const bool bTrailCloserDown = PC->IsInputKeyDown(TrailCloserKey);
	if (bTrailCloserDown && !bTrailCloserKeyWasDown)
	{
		if (RailCamera)
		{
			RailCamera->AdjustTrailDistance(-1.0f);
		}
	}
	bTrailCloserKeyWasDown = bTrailCloserDown;

	const bool bTrailFartherDown = PC->IsInputKeyDown(TrailFartherKey);
	if (bTrailFartherDown && !bTrailFartherKeyWasDown)
	{
		if (RailCamera)
		{
			RailCamera->AdjustTrailDistance(1.0f);
		}
	}
	bTrailFartherKeyWasDown = bTrailFartherDown;

	// Player speed keys: each press steps the WASD drive force (O down, P up).
	const bool bSpeedDownDown = PC->IsInputKeyDown(SpeedDownKey);
	if (bSpeedDownDown && !bSpeedDownKeyWasDown)
	{
		AdjustDriveSpeed(-1.0f);
	}
	bSpeedDownKeyWasDown = bSpeedDownDown;

	const bool bSpeedUpDown = PC->IsInputKeyDown(SpeedUpKey);
	if (bSpeedUpDown && !bSpeedUpKeyWasDown)
	{
		AdjustDriveSpeed(1.0f);
	}
	bSpeedUpKeyWasDown = bSpeedUpDown;

	// Set-axis keys 1/2/3 snap gravity to the positive direction of that axis.
	const bool bAxisSetXDown = PC->IsInputKeyDown(AxisSetXKey);
	if (bAxisSetXDown && !bAxisSetXWasDown)
	{
		HandleSetGravityAxis(EGSGravityAxis::X);
	}
	bAxisSetXWasDown = bAxisSetXDown;

	const bool bAxisSetYDown = PC->IsInputKeyDown(AxisSetYKey);
	if (bAxisSetYDown && !bAxisSetYWasDown)
	{
		HandleSetGravityAxis(EGSGravityAxis::Y);
	}
	bAxisSetYWasDown = bAxisSetYDown;

	const bool bAxisSetZDown = PC->IsInputKeyDown(AxisSetZKey);
	if (bAxisSetZDown && !bAxisSetZWasDown)
	{
		HandleSetGravityAxis(EGSGravityAxis::Z);
	}
	bAxisSetZWasDown = bAxisSetZDown;
}

void AGSRollingBallPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 转向器过渡先于移动/相机推进:三者读到同一个平滑重力方向。
	UpdateGravityRedirect(DeltaSeconds);

	if (bInputLocked)
	{
		// A center-screen message is up: suppress all gameplay input (roll/flip/
		// interact/reset/axis). The ball is not force-frozen - it simply gets no
		// move input, so it coasts/brakes to rest naturally. Only the dismiss key
		// is watched while locked.
		const APlayerController* PC = Cast<APlayerController>(GetController());
		const bool bDismiss = PC && PC->IsInputKeyDown(DismissMessageKey);
		if (bDismiss && !bDismissKeyWasDown)
		{
			DismissPendingMessage();
		}
		bDismissKeyWasDown = bDismiss;

		if (!bInputLocked)
		{
			// Dismissed this frame - fall through to a normal pass so a held
			// movement key resumes immediately.
		}
		else
		{
			ApplyMovement(DeltaSeconds);
			UpdateCamera(DeltaSeconds);
			return;
		}
	}
	else
	{
		bDismissKeyWasDown = false;
	}

	PollNativeInput();
	ApplyMovement(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
}

void AGSRollingBallPawn::ShowMessageAndLock(const FText& Message)
{
	PendingMessage = Message;
	bInputLocked = true;
	SetMoveInput(FVector2D::ZeroVector);
}

void AGSRollingBallPawn::DismissPendingMessage()
{
	if (bInputLocked)
	{
		bInputLocked = false;
		PendingMessage = FText::GetEmpty();
	}
}

bool AGSRollingBallPawn::IsMessageLocked() const
{
	return bInputLocked;
}

FText AGSRollingBallPawn::GetPendingMessage() const
{
	return PendingMessage;
}

FKey AGSRollingBallPawn::GetMessageDismissKey() const
{
	return DismissMessageKey;
}

void AGSRollingBallPawn::SetMoveInput(FVector2D NewMoveInput)
{
	MoveInput = NewMoveInput;
}

void AGSRollingBallPawn::AddCameraLookInput(float YawDeltaDegrees, float PitchDeltaDegrees)
{
	// 航向绕"当前重力上轴"旋转:上轴 ±Z 互换时同一鼠标动作给出的屏幕转向天然一致
	//(绕 -Z 转 +θ 等价于绕 +Z 转 -θ,恰好抵消旧实现的镜像)。
	if (!CameraAimHeading.IsNearlyZero() && YawDeltaDegrees != 0.0f)
	{
		const FVector Axis = CurrentCameraUp.GetSafeNormal();
		if (!Axis.IsNearlyZero())
		{
			CameraAimHeading = FQuat(Axis, FMath::DegreesToRadians(YawDeltaDegrees))
				.RotateVector(CameraAimHeading);
		}
	}
	CameraPitchDegrees = FMath::Clamp(CameraPitchDegrees + PitchDeltaDegrees, -MaximumCameraPitchDegrees, MaximumCameraPitchDegrees);
}

void AGSRollingBallPawn::HandleFlipPressed()
{
	// 转向器滑行期间忽略 G:过渡由转向器接管,中途打断会让方向与弯道脱节。
	if (bGravityRedirectActive)
	{
		return;
	}
	RequestManualGravityFlip();
}

EGSGravityRequestResult AGSRollingBallPawn::RequestManualGravityFlip()
{
	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	const EGSGravityRequestResult Result = GravityManager->RequestToggleGravity(this, EGSGravityChangeReason::MANUAL, false);
	if (Result == EGSGravityRequestResult::ACCEPTED && BallCollision)
	{
		const FVector Velocity = BallCollision->GetPhysicsLinearVelocity();
		BallCollision->SetPhysicsLinearVelocity(Velocity * ManualVelocityRetention);
	}

	return Result;
}

EGSGravityRequestResult AGSRollingBallPawn::RequestGravityPolarity(EGSGravityPolarity NewPolarity, bool bForce)
{
	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	return GravityManager->RequestGravityPolarity(NewPolarity, this, EGSGravityChangeReason::SCRIPTED, bForce);
}

EGSGravityRequestResult AGSRollingBallPawn::RequestGravityDirection(EGSGravityDirection NewDirection, bool bForce)
{
	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	return GravityManager->RequestGravityDirection(NewDirection, this, EGSGravityChangeReason::SCRIPTED, bForce);
}

EGSGravityDirection AGSRollingBallPawn::GetCurrentGravityDirection() const
{
	return GravityManager ? GravityManager->GetCurrentDirection() : EGSGravityDirection::NEGATIVE_Z;
}

void AGSRollingBallPawn::HandleSetGravityAxis(EGSGravityAxis Axis)
{
	if (!GravityManager)
	{
		return;
	}

	// 转向器滑行期间忽略 1/2/3,理由同 G。
	if (bGravityRedirectActive)
	{
		return;
	}

	// 1/2/3 snap gravity to the positive direction of the pressed axis. Pressing
	// the already-active axis yields NO_CHANGE, which is harmless.
	const EGSGravityRequestResult Result = GravityManager->SetGravityAxis(Axis, this, false);
	if (Result == EGSGravityRequestResult::REJECTED_DISABLED)
	{
		ShowAxisDisabledHint(Axis);
	}
}

void AGSRollingBallPawn::ShowAxisDisabledHint(EGSGravityAxis Axis)
{
	AxisHintText = FString::Printf(TEXT("%s轴不可用"), *GSGravity::GetAxisDisplayName(Axis));
	AxisHintExpireTime = GetWorld() ? GetWorld()->GetTimeSeconds() + AxisHintLifetimeSeconds : -1.0f;
}

bool AGSRollingBallPawn::IsAxisHintActive() const
{
	const UWorld* World = GetWorld();
	return World && AxisHintExpireTime >= 0.0f && World->GetTimeSeconds() < AxisHintExpireTime;
}

FString AGSRollingBallPawn::GetAxisHintText() const
{
	return AxisHintText;
}

AActor* AGSRollingBallPawn::FindBestInteractable() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistanceSq = InteractionRadiusCm * InteractionRadiusCm;

	APawn* SelfPawn = const_cast<AGSRollingBallPawn*>(this);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == this || !Candidate->GetClass()->ImplementsInterface(UGSInteractable::StaticClass()))
		{
			continue;
		}

		// Skip candidates that currently refuse interaction (collected-and-hidden
		// pickups, exhausted switches) so they don't shadow a valid target nearby.
		if (!IGSInteractable::Execute_CanInteract(Candidate, SelfPawn))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}

	return Best;
}

bool AGSRollingBallPawn::TryInteract()
{
	AActor* Target = FindBestInteractable();
	if (!Target)
	{
		return false;
	}

	if (IGSInteractable::Execute_CanInteract(Target, this))
	{
		return IGSInteractable::Execute_Interact(Target, this);
	}

	return false;
}

FText AGSRollingBallPawn::GetCurrentInteractionText() const
{
	AActor* Target = FindBestInteractable();
	if (Target && IGSInteractable::Execute_CanInteract(Target, const_cast<AGSRollingBallPawn*>(this)))
	{
		return IGSInteractable::Execute_GetInteractionText(Target, const_cast<AGSRollingBallPawn*>(this));
	}

	return FText::GetEmpty();
}

void AGSRollingBallPawn::AdjustDriveSpeed(float Direction)
{
	DriveAccelerationCm = FMath::Clamp(
		DriveAccelerationCm + Direction * DriveAdjustStepCm, DriveMinCm, DriveMaxCm);
}

void AGSRollingBallPawn::ResetToCheckpoint()
{
	if (GravityManager)
	{
		GravityManager->ResetGravity(true);
	}

	if (Resettable)
	{
		if (WorldStateManager && WorldStateManager->HasActiveCheckpoint)
		{
			Resettable->TeleportAndReset(WorldStateManager->ActiveCheckpointTransform);
		}
		else
		{
			Resettable->RestoreInitialState();
		}
	}

	if (LandingResponse)
	{
		LandingResponse->ResetFlightState();
	}
}

FVector AGSRollingBallPawn::GetBallLinearVelocity() const
{
	return BallCollision ? BallCollision->GetPhysicsLinearVelocity() : FVector::ZeroVector;
}

void AGSRollingBallPawn::SetBallLinearVelocity(FVector NewVelocity, bool bAddToCurrent)
{
	if (!BallCollision)
	{
		return;
	}

	const FVector Final = bAddToCurrent ? BallCollision->GetPhysicsLinearVelocity() + NewVelocity : NewVelocity;
	BallCollision->SetPhysicsLinearVelocity(Final);
}

FVector AGSRollingBallPawn::GetCameraUpVector() const { return CurrentCameraUp; }
FVector AGSRollingBallPawn::GetTargetCameraUpVector() const { return TargetCameraUp; }

EGSGravityPolarity AGSRollingBallPawn::GetCurrentGravityPolarity() const
{
	return GravityManager ? GravityManager->CurrentPolarity : EGSGravityPolarity::NEGATIVE_Z;
}

bool AGSRollingBallPawn::DoesGravityFlipRotateBall() const { return false; }

USphereComponent* AGSRollingBallPawn::GetBallCollisionComponent() const { return BallCollision; }
UCameraComponent* AGSRollingBallPawn::GetBallCameraComponent() const { return Camera; }

void AGSRollingBallPawn::HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason)
{
	// Camera target only. The actor, collision sphere and mesh are never reoriented here,
	// so natural rolling angular state survives the flip.
	TargetCameraUp = bCameraFlipsWithGravity ? -GravityDirection : FVector::UpVector;
}
