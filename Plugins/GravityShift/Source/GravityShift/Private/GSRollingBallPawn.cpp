#include "GSRollingBallPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

#include "GSBlockBase.h"
#include "GSGravityBodyComponent.h"
#include "GSGravityManager.h"
#include "GSInteractable.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
#include "GSRailCameraComponent.h"
#include "GSResettableComponent.h"
#include "GSSettingsSaveGame.h"
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

	// 贴脸遮挡用的半透明材质(插件 Content,带 Opacity 标量参数)。缺失时退回老行为(整球隐藏)。
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FadeMatAsset(TEXT("/GravityShift/Materials/M_GS_BallFade.M_GS_BallFade"));
	if (FadeMatAsset.Succeeded())
	{
		BallMeshFadeMaterial = FadeMatAsset.Object;
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

	// 用户灵敏度设置:开局读一次存档缓存到成员里,轮询每帧用缓存,不碰磁盘。
	MouseSensitivityMultiplier = UGSSettingsSaveGame::LoadOrCreate()->MouseSensitivityMultiplier;

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

// ============================ 特殊滑梯:面吸附 ============================
// 和"弯道滑行"是两套算法:弯道滑行假设球已经骑在弯道某一面上、沿 90° 弧滚出去,方向由
// "入口上行轴×出口上行轴"推导;面吸附处理的是"球碰到竖直面(垂直于地面的那一面)"这一下:
// 碰到就吸附,沿曲面切向把它自然带到平面(平行地面的那一面),走到平面时重力正好转到位。
// 触发由 UGSRedirectorComponent 的 bFaceCaptureMode 负责,只给关卡里那条特殊滑梯用。
void AGSRollingBallPawn::BeginFaceCapture(AActor* ChuteActor, FVector ContactNormal, FVector ExitGravity,
	float DriveSpeedCm, float StickAccelCm, float ExitNormalDot,
	bool bGrounded, float FloorSpeedCm, float CeilSpeedCm)
{
	const FVector Normal = ContactNormal.GetSafeNormal();
	const FVector Target = ExitGravity.GetSafeNormal();
	if (!ChuteActor || !BallCollision || Normal.IsNearlyZero() || Target.IsNearlyZero())
	{
		return;
	}

	// 出口面法线 = 出口重力的反向;旋转轴 = 两面法线的叉积(这段圆弧就是绕它卷的)。
	const FVector ExitNormal = -Target;
	const FVector Axis = FVector::CrossProduct(Normal, ExitNormal).GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		// 两面平行:构不成"从侧面被带到平面上",不接管。
		return;
	}

	bFaceCaptureActive = true;
	FaceCaptureActor = ChuteActor;
	FaceCaptureEntryNormal = Normal;
	FaceCaptureExitNormal = ExitNormal;
	FaceCaptureAxis = Axis;
	FaceCaptureCurrentNormal = Normal;
	FaceCaptureGravityFrom = GetActiveGravityDirection().GetSafeNormal();
	FaceCaptureGravityTo = Target;
	FaceCaptureGravityCurrent = FaceCaptureGravityFrom;
	FaceCaptureSpeedCm = FMath::Max(DriveSpeedCm, 50.0f);
	FaceCaptureStickAccelCm = FMath::Max(StickAccelCm, 0.0f);
	FaceCaptureExitNormalDot = FMath::Clamp(ExitNormalDot, 0.1f, 0.999f);
	FaceCaptureElapsedSeconds = 0.0f;
	bFaceCaptureGravityCommitted = false;
	bFaceCaptureGrounded = bGrounded;
	FaceCaptureFloorSpeedCm = FMath::Max(FloorSpeedCm, 1.0f);
	FaceCaptureCeilSpeedCm = FMath::Max(CeilSpeedCm, FaceCaptureFloorSpeedCm);
	FaceCaptureSurfaceLostAcc = 0.0f;

	// 入口法线不一定正好在"入口面法线"上:球快的时候一帧就跨过平面的那一段,是在圆弧中段
	// 被接住的。这时**已经扫过**一段弧(θ0),把它换算成路程预先记上——重力旋转才会恰好在
	// "接触法线到达出口面"的同一刻转完;否则收尾会变成"重力还没转完就被硬提交"的一跳。
	//   EntryAngleDeg = 入口法线到**出口**法线的夹角 = 还**剩**多少度没扫(不是扫过的!)
	//   扫过的 θ0 = 90° − EntryAngleDeg
	// (⚠ 首版写反过:直接拿 EntryAngleDeg 当初值 → 一进吸附重力就跳完 87% 再慢慢补,
	//  相机在进入瞬间被猛拽一下;2026-09-16 高速实测 entryAngle=79° 时暴露。)
	const float EntryAngleDeg = FMath::RadiansToDegrees(
		FMath::Acos(FMath::Clamp(FVector::DotProduct(Normal, ExitNormal), -1.0f, 1.0f)));
	const float SweptAngleDeg = FMath::Clamp(90.0f - EntryAngleDeg, 0.0f, 90.0f);
	FaceCapturePathCm = FaceCaptureRotateOverCm * (SweptAngleDeg / 90.0f);

	// 进入这一帧就把球摁到面上并换上"沿面前进"的速度:否则球还带着离开面的速度,
	// 会先弹出去再被拉回来,看起来就是"卡一下"。
	// 温和吸附(球自己滚进圆弧)不能这么干——它本来就在好好滚,定速会把它拽一下;
	// 只保留"沿面"那一半速度、切掉"正在离开面"的那一半即可。
	const FVector Tangent = FVector::CrossProduct(FaceCaptureAxis, FaceCaptureCurrentNormal).GetSafeNormal();
	const float EntrySpeedCm = BallCollision->GetPhysicsLinearVelocity().Size();
	if (BallCollision->IsSimulatingPhysics())
	{
		BallCollision->WakeRigidBody();
		if (!Tangent.IsNearlyZero())
		{
			if (bFaceCaptureGrounded)
			{
				const FVector EntryVelocity = BallCollision->GetPhysicsLinearVelocity();
				const float TangentialSpeed = FMath::Clamp(FVector::DotProduct(EntryVelocity, Tangent),
					FaceCaptureFloorSpeedCm, FaceCaptureCeilSpeedCm);
				const float InwardSpeed = FMath::Min(FVector::DotProduct(EntryVelocity, FaceCaptureCurrentNormal), 0.0f);
				BallCollision->SetPhysicsLinearVelocity(Tangent * TangentialSpeed + FaceCaptureCurrentNormal * InwardSpeed);
			}
			else
			{
				BallCollision->SetPhysicsLinearVelocity(Tangent * FaceCaptureSpeedCm);
			}
		}
	}
	if (GravityBody)
	{
		GravityBody->SetGravityDirectionOverride(FaceCaptureGravityCurrent);
	}

	if (bFaceCaptureDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[GSFaceCapture] begin chute=%s style=%s n=(%.2f,%.2f,%.2f) exitG=(%.2f,%.2f,%.2f) axis=(%.2f,%.2f,%.2f) speed=%.0f floor=%.0f ceil=%.0f entryAngle=%.0fdeg entryV=%.0f"),
			*GetNameSafe(ChuteActor), bFaceCaptureGrounded ? TEXT("grounded") : TEXT("hard"),
			Normal.X, Normal.Y, Normal.Z,
			Target.X, Target.Y, Target.Z, Axis.X, Axis.Y, Axis.Z,
			FaceCaptureSpeedCm, FaceCaptureFloorSpeedCm, FaceCaptureCeilSpeedCm,
			EntryAngleDeg, EntrySpeedCm);
	}
}

void AGSRollingBallPawn::EndFaceCapture()
{
	if (!bFaceCaptureActive)
	{
		return;
	}

	// 重力补完再放开:中途释放会留下一个介于两个面之间的重力方向。
	FaceCaptureGravityCurrent = FaceCaptureGravityTo;
	if (!bFaceCaptureGravityCommitted && GravityManager)
	{
		bFaceCaptureGravityCommitted = true;
		GravityManager->RequestGravityDirection(
			GSGravity::VectorToDirection(FaceCaptureGravityTo), this, EGSGravityChangeReason::SCRIPTED, true);
	}
	if (GravityBody)
	{
		GravityBody->SetGravityDirectionOverride(FVector::ZeroVector);
	}

	bFaceCaptureActive = false;
	FaceCaptureActor = nullptr;
	FaceCapturePathCm = 0.0f;
	FaceCaptureElapsedSeconds = 0.0f;
	FaceCaptureReleaseTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	if (bFaceCaptureDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[GSFaceCapture] end gravity=(%.2f,%.2f,%.2f)"),
			FaceCaptureGravityTo.X, FaceCaptureGravityTo.Y, FaceCaptureGravityTo.Z);
	}
}

FVector AGSRollingBallPawn::GetFaceCaptureNormal() const
{
	return bFaceCaptureActive ? FaceCaptureCurrentNormal : FVector::ZeroVector;
}

bool AGSRollingBallPawn::IsFaceCaptureCoolingDown() const
{
	if (FaceCaptureReleaseCooldownSeconds <= 0.0f)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	return (World->GetTimeSeconds() - FaceCaptureReleaseTime) < FaceCaptureReleaseCooldownSeconds;
}

void AGSRollingBallPawn::UpdateFaceCapture(float DeltaSeconds)
{
	if (!bFaceCaptureActive)
	{
		return;
	}

	const float Dt = FMath::Clamp(DeltaSeconds, 0.0f, 0.1f);
	FaceCaptureElapsedSeconds += Dt;

	if (!BallCollision || !BallCollision->IsSimulatingPhysics() || !FaceCaptureActor)
	{
		EndFaceCapture();
		return;
	}

	const float Radius = BallCollision->GetScaledSphereRadius();
	const FVector BallLoc = BallCollision->GetComponentLocation();

	// 1) 沿"当前接触法线"反向探面,取回真实曲面法线。凸圆弧上球一离面,理想弧线就带不住
	//    它了——真实法线才是驱动方向,这一步是"贴着布尔网格走完"的关键(与滑行同源)。
	float GapCm = 0.0f;
	bool bHasSurface = false;
	if (const UWorld* World = GetWorld())
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GSFaceCaptureProbe), false, this);
		if (World->LineTraceSingleByChannel(Hit, BallLoc,
			BallLoc - FaceCaptureCurrentNormal * (Radius + FaceCaptureProbeReachCm), ECC_Visibility, Params))
		{
			const FVector HitNormal = Hit.Normal.GetSafeNormal();
			// 本滑梯件的面直接用;**不是本滑梯件**但法线跟当前法线足够连续(同一张连续曲面的
			// 邻接网格,例如墙角那面墙、叠着摆的另一个滑梯件)也接受 —— 见头文件说明:
			// 否则球绕到墙角时法线冻结、出口判定永远不满足,会被恒速甩飞。
			if (Hit.GetActor() == FaceCaptureActor
				|| FVector::DotProduct(HitNormal, FaceCaptureCurrentNormal) >= FaceCaptureSurfaceNormalMinDot)
			{
				bHasSurface = true;
				GapCm = Hit.Distance - Radius;
				FaceCaptureCurrentNormal = HitNormal;
			}
		}
	}

	// 连续面也找不到的累计时长:超过阈值就放开(补完重力),不让"法线冻结+恒速切向"把球甩飞。
	FaceCaptureSurfaceLostAcc = bHasSurface ? 0.0f : (FaceCaptureSurfaceLostAcc + Dt);
	if (FaceCaptureSurfaceLostAcc >= FaceCaptureSurfaceLostSeconds)
	{
		EndFaceCapture();
		if (bFaceCaptureDebugLog)
		{
			UE_LOG(LogTemp, Log, TEXT("[GSFaceCapture] release reason=surface-lost(both own & continuous) elapsed=%.2fs"),
				FaceCaptureElapsedSeconds);
		}
		return;
	}

	// 2) 沿面切向 = (旋转轴 × 当前法线):入口竖直面时朝上,出口平面时朝外,
	//    方向随法线连续旋转——"被自然而然地带到平面上"就是这一条。
	const FVector Tangent = FVector::CrossProduct(FaceCaptureAxis, FaceCaptureCurrentNormal).GetSafeNormal();
	if (Tangent.IsNearlyZero())
	{
		EndFaceCapture();
		return;
	}

	// 3) 速度 = 切向驱动 + 法向合拢(离面就把球拉回来;凸面上被甩开的趋势也吃这一步)。
	//    温和吸附里"切向驱动"不是定速,而是球自己的切向速度钳进 [floor, ceil]:球滚得快就
	//    走快、重力也随路程转得快(不会出现"球还在坡上重力已转完"),滚得慢就由下限兜底。
	//    法向只切掉"正在离开面"的那一半(>0 的分量),再加一点随间隙的合拢——比硬吸附轻,
	//    但同样不允许它自己飞离弧面。
	float DriveSpeedCm = FaceCaptureSpeedCm;
	float NormalSpeed = 0.0f;
	if (bFaceCaptureGrounded)
	{
		const FVector CurrentVelocity = BallCollision->GetPhysicsLinearVelocity();
		DriveSpeedCm = FMath::Clamp(FVector::DotProduct(CurrentVelocity, Tangent),
			FaceCaptureFloorSpeedCm, FaceCaptureCeilSpeedCm);
		NormalSpeed = FMath::Min(FVector::DotProduct(CurrentVelocity, FaceCaptureCurrentNormal), 0.0f);
		if (bHasSurface)
		{
			// **这一帧就把间隙合上**(而不是按间隙比例给一点力)。凸圆弧上球每帧会按 v²/ρ 往外
			// 飘——900cm/s、R≈111 时约 0.7cm/帧;旧写法(Gap*Gain, gain=8)只能补 0.1cm/帧,
			// 追不上,球就会越飘越远直到脱面(用户反馈"跑太快会飞出去"的主因)。
			// 负间隙(压进面里)时 -Gap/Dt 为正,被 min 挡住 → 不会往外推,只收不放。
			NormalSpeed = FMath::Min(NormalSpeed, -GapCm / Dt);
			NormalSpeed = FMath::Max(NormalSpeed, -FaceCaptureCeilSpeedCm);
		}
	}
	else
	{
		NormalSpeed = bHasSurface
			? FMath::Clamp(-GapCm * FaceCaptureGapGain, -FaceCaptureSpeedCm, FaceCaptureSpeedCm)
			: 0.0f;
	}
	BallCollision->WakeRigidBody();
	BallCollision->SetPhysicsLinearVelocity(Tangent * DriveSpeedCm + FaceCaptureCurrentNormal * NormalSpeed);
	if (FaceCaptureStickAccelCm > 0.0f)
	{
		BallCollision->AddForce(-FaceCaptureCurrentNormal * FaceCaptureStickAccelCm, NAME_None, true);
	}

	// 4) 重力按走过的路程旋转(与转向器滑行同一节奏:走到平面 = 正好转到位)。
	FaceCapturePathCm += DriveSpeedCm * Dt;
	const float Alpha = FMath::Clamp(FaceCapturePathCm / FMath::Max(FaceCaptureRotateOverCm, 1.0f), 0.0f, 1.0f);
	const float Eased = Alpha * Alpha * (3.0f - 2.0f * Alpha);
	const FQuat Delta = FQuat::FindBetweenNormals(FaceCaptureGravityFrom, FaceCaptureGravityTo);
	FaceCaptureGravityCurrent = FQuat::Slerp(FQuat::Identity, Delta, Eased).RotateVector(FaceCaptureGravityFrom);
	if (GravityBody)
	{
		GravityBody->SetGravityDirectionOverride(FaceCaptureGravityCurrent);
	}

	// 5) 出口:接触法线转到"出口面朝上"→ 提交重力并释放,球带着切向速度滚上平面。
	//    超时是保险(面被卡住/组件异常时不会永久夺走玩家控制)。
	const float ExitDot = FVector::DotProduct(FaceCaptureCurrentNormal, FaceCaptureExitNormal);
	const bool bReachedExit = ExitDot >= FaceCaptureExitNormalDot;
	const bool bTimedOut = FaceCaptureElapsedSeconds >= FaceCaptureMaxSeconds;
	if (bReachedExit || bTimedOut)
	{
		const float ElapsedForLog = FaceCaptureElapsedSeconds;
		EndFaceCapture();
		if (bFaceCaptureDebugLog)
		{
			UE_LOG(LogTemp, Log, TEXT("[GSFaceCapture] release dot=%.2f elapsed=%.2fs reason=%s"),
				ExitDot, ElapsedForLog, bReachedExit ? TEXT("reached-plane") : TEXT("timeout"));
		}
		return;
	}

	if (bFaceCaptureDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[GSFaceCapture] t=%.2f ball=(%.0f,%.0f,%.0f) n=(%.2f,%.2f,%.2f) tan=(%.2f,%.2f,%.2f) g=(%.2f,%.2f,%.2f) gap=%.0f surf=%d prog=%.0f/%.0f"),
			FaceCaptureElapsedSeconds, BallLoc.X, BallLoc.Y, BallLoc.Z,
			FaceCaptureCurrentNormal.X, FaceCaptureCurrentNormal.Y, FaceCaptureCurrentNormal.Z,
			Tangent.X, Tangent.Y, Tangent.Z,
			FaceCaptureGravityCurrent.X, FaceCaptureGravityCurrent.Y, FaceCaptureGravityCurrent.Z,
			GapCm, bHasSurface ? 1 : 0,
			FaceCapturePathCm, FaceCaptureRotateOverCm);
	}
}

// 与上轴最不平行的世界参考轴。
// ⚠ 2026-09-15 用户"墙态重力下完全转不动"的根因:参考硬编码成世界 +X,而上轴正好是 ±X
// (球在竖墙上、重力 ±X)时,"+X 投影到 ⊥上轴平面"= 零向量 → 相机姿态算废 → 画面乱转/不转。
static FVector GSPickReferenceAxis(const FVector& Up)
{
	const FVector Axes[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
	int32 BestIdx = 0;
	float BestAlign = 2.0f;
	for (int32 i = 0; i < 3; ++i)
	{
		const float Align = FMath::Abs(FVector::DotProduct(Axes[i], Up));
		if (Align < BestAlign)
		{
			BestAlign = Align;
			BestIdx = i;
		}
	}
	return Axes[BestIdx];
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
		// 航向几乎平行于上轴(比如墙态重力):退化到"与上轴最不平行的世界轴"的投影。
		const FVector Ref = GSPickReferenceAxis(Up);
		Forward = (Ref - Up * FVector::DotProduct(Ref, Up)).GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::RightVector;
		}
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
	// 抖动主因)。改为:沿"枢轴→期望相机位"打 ECC_Camera 探针;命中立即压入(防穿墙),
	// 只有连续无命中 ArmExtendHoldSeconds 后才缓慢放长——翻转被去弹,相机稳定不再弹跳。
	// 2026-09-13(用户反馈"容易穿模")两处加固:①探针必须沿相机真实最终朝向打(含
	// AimPitchDeg 瞄准俯仰微调;旧实现用 CurrentCameraRotation,视线偏最多 ~17°,
	// 探针判"安全"时相机实际已进墙);②球扫掠替代单线 + 命中立即压入替代 8/s 平滑
	// 收短(平滑期间相机停在墙里的那几帧就是穿模)。
	const FVector PivotLoc = CameraPivot->GetComponentLocation();
	const FQuat ProbeRotation = BuildCameraRotation(TargetCameraUp, LastAimPitchDeg);
	const FVector ArmDir = -ProbeRotation.GetForwardVector();
	const FVector DesiredCamPos = PivotLoc + ArmDir * CameraArmLengthCm;
	float SafeArmCm = CameraArmLengthCm;
	FString ArmHitName = TEXT("none");
	{
		FHitResult ArmHit;
		FCollisionQueryParams ArmParams(SCENE_QUERY_STAT(GSFallbackCamProbe), false, this);
		if (GetWorld()->SweepSingleByChannel(ArmHit, PivotLoc, DesiredCamPos, FQuat::Identity,
			ECC_Camera, FCollisionShape::MakeSphere(CameraProbeRadiusCm), ArmParams))
		{
			SafeArmCm = FMath::Max((ArmHit.Location - PivotLoc).Size() - CameraProbeMarginCm, 40.0f);
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
	// 目标臂长恒为"探针允许的最大臂长" SafeArmCm(全空=满臂)。注意不能只写
	// Min(当前,SafeArm):SafeArm 未满但比当前臂更远时(离开墙缝)臂会永远涨不回去
	// ——首次实现就踩了这个死锁(臂卡 40,相机贴脸+球被隐藏)。
	if (SafeArmCm < SmoothedArmLengthCm)
	{
		// 障碍比当前臂更近:立即压入(不做时间平滑——平滑期间相机停在墙里就是"穿模")。
		ProbeClearSeconds = 0.0f;
		SmoothedArmLengthCm = SafeArmCm;
	}
	else
	{
		// 已处于安全范围:按"驻留 + 限速"平滑放长到 SafeArm(未满臂时同样适用)。
		// 棱边探针逐帧翻转(命中帧不断把驻留清零)在此被去弹,不会来回抽。
		ProbeClearSeconds += DeltaSeconds;
		// 瞄准解除后的 1.2s 内走"快速回弹":不等 0.7s 驻留、放长速度 ×3,
		// 松开右键后视线一离开天花板/墙就立刻回到正常距离。
		const bool bFastExtend = GetWorld() && GetWorld()->GetTimeSeconds() < FastArmExtendUntilSeconds;
		if (ProbeClearSeconds > (bFastExtend ? 0.0f : ArmExtendHoldSeconds))
		{
			SmoothedArmLengthCm = FMath::FInterpTo(SmoothedArmLengthCm, SafeArmCm, DeltaSeconds,
				bFastExtend ? FMath::Max(ArmLengthInterpSpeed * 3.0f, 12.0f)
					: FMath::Max(ArmLengthInterpSpeed, 0.1f));
		}
	}
	CameraArm->TargetArmLength = SmoothedArmLengthCm;

	// 球贴脸遮挡:球在画面上占太大时把球调成半透明让出视野(2026-09-15 用户反馈
	// "挡住屏幕大块部分却不变半透明"——只看臂长不够:球大/视场窄时臂还很长就已经糊住
	// 半屏)。改判**角半径** asin(球半径/相机到球距离),与臂长阈值是"或"关系;滞回防抖。
	// 2026-09-15 二次修订:原实现"瞄准态不处理",而瞄准臂只有 250cm 且 FOV 收窄——球又大
	// 又近,恰恰是最挡视野的时机,用户实测"按住右键挡住大半屏却不淡出"。改为瞄准态同样淡出
	// (bFadeBallWhileAiming,默认开;关掉即回到旧行为)。纯视觉,不影响物理/瞄准判定。
	if (BallMesh)
	{
		const FVector CamLoc = CameraArm ? CameraArm->GetSocketLocation(NAME_None) : GetActorLocation();
		const float DistToBallCm = FMath::Max((BallLoc - CamLoc).Size(), 1.0f);
		const float BallRadiusCm = BallCollision ? BallCollision->GetScaledSphereRadius() : 40.0f;
		const float BallAngleDeg = FMath::RadiansToDegrees(
			FMath::Asin(FMath::Clamp(BallRadiusCm / DistToBallCm, 0.0f, 1.0f)));
		// 视场越窄,同样的角半径占屏越大(瞄准 ADS 会把 FOV 收窄):阈值按 FOV/90 等比收,
		// 否则"瞄准时球糊住半屏"仍然不触发。相机缺失时按 90° 基准。
		const float FovDeg = Camera ? FMath::Max(Camera->FieldOfView, 20.0f) : 90.0f;
		const float FadeAngleScale = FMath::Clamp(FovDeg / 90.0f, 0.45f, 1.4f);
		const bool bBallTooBig = BallAngleDeg >= BallMeshFadeStartAngleDeg * FadeAngleScale
			|| SmoothedArmLengthCm < BallMeshHideBelowArmCm;
		const bool bBallSmallEnough = BallAngleDeg <= BallMeshFadeEndAngleDeg * FadeAngleScale
			&& SmoothedArmLengthCm > BallMeshShowAboveArmCm;
		// 瞄准且关掉了"瞄准也淡出"时,恢复原材质(不能在瞄准中保持半透明)。
		const bool bFadeSuppressed = bAiming && !bFadeBallWhileAiming;
		if (!bFadeSuppressed && !bBallMeshHidden && bBallTooBig)
		{
			bBallMeshHidden = true;
			SetBallMeshFaded(true);
		}
		else if (bBallMeshHidden && (bFadeSuppressed || bBallSmallEnough))
		{
			bBallMeshHidden = false;
			SetBallMeshFaded(false);
		}
	}

	const float LiftScale = FMath::Clamp(SmoothedArmLengthCm / FMath::Max(CameraArmLengthCm, 1.0f), 0.0f, 1.0f);
	// 挤窄兜底:臂越短,额外抬得越高(球糊屏 → 改成从球上方俯看,并绕开背后的墙)。
	const float SqueezeScale = 1.0f - LiftScale;
	const float SqueezeLiftCm = SqueezeScale * CameraSqueezeLiftCm;
	// 挤窄时的视场补偿:拉宽 FOV 让玩家还能看见周围(不动俯仰、不穿模)。写在 UpdateCamera
	// 里、位于 UpdateAiming 之后,所以这里最后写谁生效的约定同样适用。
	if (Camera && CameraSqueezeFovBoostDeg > 0.0f)
	{
		const float BaseFov = bAiming ? AimTargetFOV : DefaultCameraFOV;
		const float SqueezeFov = BaseFov + SqueezeScale * CameraSqueezeFovBoostDeg;
		Camera->FieldOfView = FMath::FInterpTo(Camera->FieldOfView, SqueezeFov, DeltaSeconds, 6.0f);
	}
	const FVector TargetPivot = BallLoc
		+ CameraLiftDirection * (CameraPivotLiftHeightCm * LiftScale + SqueezeLiftCm);
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
		// 同上:上轴 = ±X 时不能拿 +X 当参考(投影为零 → 姿态/俯仰全废)。
		const FVector Ref = GSPickReferenceAxis(TargetCameraUp);
		HeadingHoriz = (Ref - TargetCameraUp * FVector::DotProduct(Ref, TargetCameraUp)).GetSafeNormal();
		if (HeadingHoriz.IsNearlyZero())
		{
			HeadingHoriz = FVector::RightVector;
		}
	}
	// **每帧**把"去掉竖直分量"的航向存回(投影是幂等操作,不会改变正常航向的方向):
	// 不这么做的话,航向在被"绕当前(俯仰/过渡中倾斜的)上轴"旋转时会慢慢积累竖直分量,
	// 最终与上轴近乎平行 → 偏航退化(用户实测"往右拽不动、只能往左")。2026-09-15。
	CameraAimHeading = HeadingHoriz;
	// 用平滑后的实际臂长(不是配置的 700):臂长塌缩时瞄准点仍落在同一世界位置,
	// 画面俯仰不会随探针收短而摆动(爬楼梯时相机贴近,旧写法会甩一下)。
	const FVector CamPosApprox = Location - HeadingHoriz * SmoothedArmLengthCm;
	const FVector LookDir = (AimTarget - CamPosApprox).GetSafeNormal();
	float AimPitchDeg = FMath::RadiansToDegrees(
		FMath::Asin(FMath::Clamp(FVector::DotProduct(LookDir, TargetCameraUp), -1.0f, 1.0f)));
	// 挤窄时衰减瞄准俯仰(见头文件 CameraSqueezeAimPitchScale 说明):否则相机在窄处永远低头,
	// 玩家想平视/看向前方某个方向时"怎么拖都转不过去"。满臂时 LiftScale=1 → 完全不衰减。
	AimPitchDeg *= FMath::Lerp(CameraSqueezeAimPitchScale, 1.0f, LiftScale);

	if (bFallbackCamDebugLog)
	{
		// 逐帧落盘(爬楼梯抖动诊断):ballZ=物理输入,pivot=平滑后,target=平滑前,
		// cam=弹簧臂实测相机位(含探针压臂),arm/scale=探针压臂与抬升缩放,pitch=瞄准俯仰。
		const FVector CamNow = CameraArm->GetSocketLocation(NAME_None);
		// 补:重力方向 / 是否支撑 / 相机上向量 —— 排查"墙态重力下转不动"必需(2026-09-15)。
		const FVector DbgG = GetActiveGravityDirection();
		const FVector DbgCamUp = CurrentCameraRotation.RotateVector(FVector::UpVector);
		UE_LOG(LogTemp, Log, TEXT("[FallbackCam] t=%06.2f dt=%.4f ballZ=%.1f g=(%.2f,%.2f,%.2f) sup=%d camup=(%.2f,%.2f,%.2f) pivot=(%.1f,%.1f,%.1f) target=(%.1f,%.1f,%.1f) cam=(%.1f,%.1f,%.1f) arm=%.1f raw=%.1f hit=%s scale=%.2f pitch=%.2f"),
			GetWorld()->GetTimeSeconds(), DeltaSeconds,
			BallLoc.Z,
			DbgG.X, DbgG.Y, DbgG.Z,
			(LandingResponse && LandingResponse->IsSupported()) ? 1 : 0,
			DbgCamUp.X, DbgCamUp.Y, DbgCamUp.Z,
			Location.X, Location.Y, Location.Z,
			TargetPivot.X, TargetPivot.Y, TargetPivot.Z,
			CamNow.X, CamNow.Y, CamNow.Z,
			SmoothedArmLengthCm, SafeArmCm, *ArmHitName, LiftScale, AimPitchDeg);
	}
	// —— 收尾复核(2026-09-15 两轮用户反馈:①"偶尔还有穿模" ②"按住右键依然穿模")——
	// 分两段各钳各的:
	//   ① 臂长:沿**最终朝向**从**最终枢轴位置**扫(上一帧俯仰/枢轴与最终位差一个夹角+位移,
	//      就是残留穿模的几帧);两个通道都扫(有些网格只挡 Visibility)。
	//   ② **越肩偏移单独钳**:瞄准态的 SocketOffset 是**横向**位移,把臂压到最短也收不掉它
	//      (相机一直贴在侧向 65cm 的墙里)——瞄准穿模就是这一条。按探针结果缩偏移量。
	{
		const FQuat FinalRotation = BuildCameraRotation(TargetCameraUp, AimPitchDeg);
		const FVector FinalArmDir = -FinalRotation.GetForwardVector();
		FCollisionQueryParams FinalParams(SCENE_QUERY_STAT(GSCamFinalVerify), false, this);

		// ① 臂长复核
		if (GetWorld())
		{
			// 只扫 ECC_Camera:关卡里击杀体/触发体这类"只挡 Visibility 不挡 Camera"的游戏体
			// 如果也算进来,球一进区域臂长就被误收到最短(2026-09-15 "相机转不动"的另一半原因)。
			float VerifyArmCm = SmoothedArmLengthCm;
			const FVector ArmEnd = Location + FinalArmDir * (SmoothedArmLengthCm + CameraProbeMarginCm);
			{
				FHitResult VerifyHit;
				if (GetWorld()->SweepSingleByChannel(VerifyHit, Location, ArmEnd, FQuat::Identity, ECC_Camera,
					FCollisionShape::MakeSphere(CameraProbeRadiusCm), FinalParams))
				{
					VerifyArmCm = FMath::Max((VerifyHit.Location - Location).Size() - CameraProbeMarginCm, 40.0f);
				}
			}
			if (VerifyArmCm < SmoothedArmLengthCm)
			{
				SmoothedArmLengthCm = VerifyArmCm;
				CameraArm->TargetArmLength = SmoothedArmLengthCm;
				ProbeClearSeconds = 0.0f;
			}
		}

		// ② 越肩偏移复核:从"轴上的相机位"沿偏移方向扫;命中就把偏移缩到命中点以内。
		//    写回组件即生效:UpdateAiming 在本帧更早处写过偏移,相机块最后这个写覆盖它
		//    (与臂长同一套"谁最后写谁生效"的约定)。
		if (CameraArm && GetWorld())
		{
			const FVector SocketWorld = FinalRotation.RotateVector(CameraArm->SocketOffset);
			const float OffsetLenCm = SocketWorld.Size();
			if (OffsetLenCm > 0.5f)
			{
				const FVector AxisCamPos = Location + FinalArmDir * SmoothedArmLengthCm;
				const FVector OffsetDir = SocketWorld / OffsetLenCm;
				const FVector OffsetEnd = AxisCamPos + OffsetDir * (OffsetLenCm + CameraProbeMarginCm);
				float SafeOffsetCm = OffsetLenCm;
				{
					FHitResult VerifyHit;
					if (GetWorld()->SweepSingleByChannel(VerifyHit, AxisCamPos, OffsetEnd, FQuat::Identity, ECC_Camera,
						FCollisionShape::MakeSphere(CameraProbeRadiusCm), FinalParams))
					{
						SafeOffsetCm = FMath::Max((VerifyHit.Location - AxisCamPos).Size() - CameraProbeMarginCm, 0.0f);
					}
				}
				if (SafeOffsetCm < OffsetLenCm)
				{
					CameraArm->SocketOffset = CameraArm->SocketOffset * (SafeOffsetCm / OffsetLenCm);
					ProbeClearSeconds = 0.0f;
				}
			}
		}
	}

	CameraPivot->SetWorldLocationAndRotation(
		Location, BuildCameraRotation(TargetCameraUp, AimPitchDeg));
	// 供下一帧探针沿"相机真实朝向"打(1 帧滞后,避免与 LiftScale→AimPitchDeg 循环依赖)。
	LastAimPitchDeg = AimPitchDeg;
	CurrentCameraUp = CurrentCameraRotation.RotateVector(FVector::UpVector);
}

void AGSRollingBallPawn::SetBallMeshFaded(bool bFaded)
{
	if (!BallMesh)
	{
		return;
	}

	// 没配半透明材质:退回原来的"整球隐藏",功能不失效。
	if (!BallMeshFadeMaterial)
	{
		BallMesh->SetVisibility(!bFaded);
		return;
	}

	if (bFaded)
	{
		if (!BallMeshFadeMID)
		{
			BallMeshFadeMID = UMaterialInstanceDynamic::Create(BallMeshFadeMaterial, this);
		}
		if (!BallMeshFadeMID)
		{
			BallMesh->SetVisibility(false);
			return;
		}
		if (!BallMeshOriginalMaterial)
		{
			BallMeshOriginalMaterial = BallMesh->GetMaterial(0);
		}
		BallMeshFadeMID->SetScalarParameterValue(TEXT("Opacity"), BallMeshFadeOpacity);
		BallMesh->SetVisibility(true);
		BallMesh->SetMaterial(0, BallMeshFadeMID);
	}
	else if (BallMeshOriginalMaterial)
	{
		BallMesh->SetMaterial(0, BallMeshOriginalMaterial);
		BallMesh->SetVisibility(true);
	}
}

bool AGSRollingBallPawn::MatchesStairTag(const AActor* Actor, const FString& TagsCsv) const
{
	if (!Actor || TagsCsv.IsEmpty())
	{
		return false;
	}
	// 局部名别叫 Tags:AActor 自己有成员 Tags(C4458 阴影告警会被当成错误)。
	TArray<FString> TagList;
	TagsCsv.ParseIntoArray(TagList, TEXT(","), true);
	const FString Label = Actor->GetActorNameOrLabel();
	const FString ClassName = Actor->GetClass()->GetName();
	for (FString& Tag : TagList)
	{
		Tag.TrimStartAndEndInline();
		if (!Tag.IsEmpty() && (Label.Contains(Tag) || ClassName.Contains(Tag)))
		{
			return true;
		}
	}
	return false;
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

	if (bFaceCaptureActive)
	{
		// 特殊滑梯吸附期间同理:速度由 UpdateFaceCapture 逐帧锁在"沿面切向 + 朝面合拢",
		// 这里必须让路,否则刹车/输入会把球从面上拽下来。
		return;
	}
	// 楼梯吸力(2026-09-13 用户需求;2026-09-15 用户要求"只改点名的那一段楼梯"后重构):
	//   基础:踩在楼梯上给一个朝支撑面的加速度,把球"摁"在台阶上、不被棱角弹飞。只对命中
	//         StairStickNameTag(默认 "Stairs")的件生效,其它任何表面零影响。
	//   强化:命中 StairStickBoostNameTag(默认 Linear3~6 = 用户点名的那段长楼梯)时,吸力与
	//         影响区都用 Boost* 的更大值,并额外吃两道**削速度**硬约束防飞(光给力治不住
	//         "被台阶棱角顶飞":力要等速度抵消,球早飞了)。
	//   ⚠ 削速度**只在下坡趋势时**生效。爬台阶必须靠向上的速度,削了就直接爬不动
	//     (用户实测"其他楼梯都上坡上不了了");趋势(垂直速度指数均值)能区分"爬坡 vs 被顶飞",
	//     瞬时速度方向区分不了。
	if (bStairStickEnabled && BallCollision->IsSimulatingPhysics())
	{
		const FVector StickDown = GetActiveGravityDirection().GetSafeNormal();
		if (!StickDown.IsNearlyZero())
		{
			const FVector StickFrom = BallCollision->GetComponentLocation();
			const float BallR = BallCollision->GetScaledSphereRadius();
			// 垂直速度指数均值 → "现在是不是在下坡"。上坡(上升趋势)时 DescendGate=0,永不削速度。
			const float TrendAlpha = FMath::Clamp(DeltaSeconds * 6.0f, 0.0f, 1.0f);
			StairVzTrendCm = FMath::Lerp(StairVzTrendCm,
				BallCollision->GetPhysicsLinearVelocity().Z, TrendAlpha);
			const float DescendGate = FMath::Clamp(-StairVzTrendCm / 200.0f, 0.0f, 1.0f);

			FHitResult StickHit;
			FCollisionQueryParams StickParams(SCENE_QUERY_STAT(GSStairStick), false, this);
			bool bOnStairs = false;
			bool bBoosted = false;
			if (GetWorld())
			{
				// ① 基础探针:基础影响区内命中"任何楼梯件"即算在楼梯上。
				if (GetWorld()->LineTraceSingleByChannel(StickHit, StickFrom,
					StickFrom + StickDown * (BallR + StairStickProbeReachCm), ECC_Visibility, StickParams))
				{
					bOnStairs = MatchesStairTag(StickHit.GetActor(), StairStickNameTag);
				}
				// ② 强化探针(更长):基础探针没认到时才用,专门给点名楼梯更大影响区。
				if (!bOnStairs)
				{
					FHitResult BoostHit;
					if (GetWorld()->LineTraceSingleByChannel(BoostHit, StickFrom,
						StickFrom + StickDown * (BallR + StairStickBoostProbeReachCm), ECC_Visibility, StickParams))
					{
						if (MatchesStairTag(BoostHit.GetActor(), StairStickBoostNameTag))
						{
							bOnStairs = true;
							bBoosted = true;
							StickHit = BoostHit;
						}
					}
				}
				if (bOnStairs && !bBoosted)
				{
					bBoosted = MatchesStairTag(StickHit.GetActor(), StairStickBoostNameTag);
				}
			}

			if (bOnStairs)
			{
				// 记下"确实踩在楼梯上"的位置与时刻:离开后的一小块靠它继续吸。
				LastStairContactLocation = StickFrom;
				LastStairContactTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				bHasStairContact = true;
				bLastStairBoosted = bBoosted;
				if (bStairDebugLog)
				{
					const FVector DbgV = BallCollision->GetPhysicsLinearVelocity();
					UE_LOG(LogTemp, Log, TEXT("[GSStair] t=%.3f on=1 boost=%d pos=(%.0f,%.0f,%.0f) v=(%.0f,%.0f,%.0f) sup=%d trend=%.0f gate=%.2f n=(%.2f,%.2f,%.2f)"),
						GetWorld()->GetTimeSeconds(), bBoosted ? 1 : 0, StickFrom.X, StickFrom.Y, StickFrom.Z,
						DbgV.X, DbgV.Y, DbgV.Z,
						(LandingResponse && LandingResponse->IsSupported()) ? 1 : 0,
						StairVzTrendCm, DescendGate,
						StickHit.ImpactNormal.X, StickHit.ImpactNormal.Y, StickHit.ImpactNormal.Z);
				}

				// 空中(刚被台阶棱角弹起 / 冲出末端)与"骑在台阶上"分开给力:
				//   空中 —— 没有接触摩擦,朝支撑面拉一把只赚不亏,用更大的固定强度;
				//   接触 —— 分坡:上坡弱(不拖爬坡)、下坡全强度(抓地不弹)。
				const bool bAirborne = LandingResponse && !LandingResponse->IsSupported();
				if (bAirborne)
				{
					const float AirborneAccel = bBoosted
						? StairStickBoostAirborneAccelCm : StairStickAirborneAccelCm;
					BallCollision->AddForce(StickDown * AirborneAccel, NAME_None, true);
					// ② 上抛分量当场归零(仅点名楼梯 + 下坡趋势)。
					const float LiftKill = bBoosted ? (StairStickBoostLiftKill * DescendGate) : 0.0f;
					if (LiftKill > 0.0f)
					{
						const FVector V = BallCollision->GetPhysicsLinearVelocity();
						const FVector LiftDir = -StickDown;
						const float LiftSpeed = FVector::DotProduct(V, LiftDir);
						if (LiftSpeed > 0.0f)
						{
							BallCollision->SetPhysicsLinearVelocity(V - LiftDir * (LiftSpeed * LiftKill));
						}
					}
				}
				else
				{
					const FVector V0 = BallCollision->GetPhysicsLinearVelocity();
					float StickScale = 1.0f;
					const float StickSpeed = V0.Size();
					if (StickSpeed > 1.0f)
					{
						const float DownhillDot = FVector::DotProduct(V0 / StickSpeed, StickDown);
						StickScale = FMath::Lerp(StairStickUphillScale, 1.0f,
							FMath::Clamp(DownhillDot, 0.0f, 1.0f));
					}
					const float ContactAccel = bBoosted ? StairStickBoostAccelCm : StairStickAccelCm;
					BallCollision->AddForce(StickDown * ContactAccel * StickScale, NAME_None, true);
					// ③ 下坡限速(仅点名楼梯 + 下坡趋势):见头文件说明——台阶不连续,不限速必跳步。
					if (bBoosted && DescendGate > 0.0f && StairStickBoostMaxSpeedCm > 0.0f)
					{
						const FVector V = BallCollision->GetPhysicsLinearVelocity();
						const FVector Vertical = StickDown * FVector::DotProduct(V, StickDown);
						FVector Planar = V - Vertical;
						const float PlanarSpeed = Planar.Size();
						if (PlanarSpeed > StairStickBoostMaxSpeedCm)
						{
							Planar *= StairStickBoostMaxSpeedCm / PlanarSpeed;
							BallCollision->SetPhysicsLinearVelocity(Vertical + Planar);
						}
					}

					// ① 离面法向分量削掉(仅点名楼梯 + 下坡趋势)。
					const float NormalKill = bBoosted ? (StairStickBoostNormalKill * DescendGate) : 0.0f;
					if (NormalKill > 0.0f)
					{
						const FVector SurfaceNormal = StickHit.ImpactNormal.GetSafeNormal();
						const FVector V = BallCollision->GetPhysicsLinearVelocity();
						const float NormalSpeed = FVector::DotProduct(V, SurfaceNormal);
						if (NormalSpeed > 0.0f)
						{
							BallCollision->SetPhysicsLinearVelocity(
								V - SurfaceNormal * (NormalSpeed * NormalKill));
						}
					}
				}
			}
			else if (bHasStairContact)
			{
				// 端点延续吸附(2026-09-15 用户需求):上下端点往外一小块仍给吸附,防止从最后
				// 一级台阶飞出去。强度/范围按"上次踩的是不是点名楼梯"取强化值或基础值。
				const float NowSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
				const float EndMargin = bLastStairBoosted
					? StairStickBoostEndMarginCm : StairStickEndMarginCm;
				const float EndSeconds = bLastStairBoosted
					? StairStickBoostEndSeconds : StairStickEndSeconds;
				const float EndAccel = bLastStairBoosted
					? StairStickBoostEndAccelCm : StairStickEndAccelCm;
				const bool bNearStairEnd =
					FVector::Dist(StickFrom, LastStairContactLocation) <= EndMargin
					&& (NowSec - LastStairContactTime) <= EndSeconds;
				if (bNearStairEnd)
				{
					BallCollision->AddForce(StickDown * EndAccel, NAME_None, true);
					// 点名楼梯的端点外同样削上抛分量(仅下坡趋势)。
					const float LiftKill = bLastStairBoosted
						? (StairStickBoostLiftKill * DescendGate) : 0.0f;
					if (LiftKill > 0.0f)
					{
						const FVector V = BallCollision->GetPhysicsLinearVelocity();
						const FVector LiftDir = -StickDown;
						const float LiftSpeed = FVector::DotProduct(V, LiftDir);
						if (LiftSpeed > 0.0f)
						{
							BallCollision->SetPhysicsLinearVelocity(V - LiftDir * (LiftSpeed * LiftKill));
						}
					}
					if (bStairDebugLog)
					{
						const FVector DbgV = BallCollision->GetPhysicsLinearVelocity();
						UE_LOG(LogTemp, Log, TEXT("[GSStair] t=%.3f on=0 boost=%d pos=(%.0f,%.0f,%.0f) v=(%.0f,%.0f,%.0f) dToLast=%.0f dt=%.2f gate=%.2f"),
							NowSec, bLastStairBoosted ? 1 : 0, StickFrom.X, StickFrom.Y, StickFrom.Z,
							DbgV.X, DbgV.Y, DbgV.Z,
							FVector::Dist(StickFrom, LastStairContactLocation),
							NowSec - LastStairContactTime, DescendGate);
					}
				}
				else
				{
					// 走远/超时 → 记忆失效:楼梯吸力绝不会漏到别的表面上。
					bHasStairContact = false;
				}
			}
		}
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

	if (CameraPivot && bAdaptiveDriveBasis)
	{
		// 相机角度自适应控制基(2026-09-13 用户定则):视线与支撑面越"正面相对"
		// (FaceOn=|视线·支撑上|→1),控制越向"屏幕相对"过渡——W 从"视线在面内的投影"
		// 渐变到"屏幕上方向在面内的投影"(正对墙面时 W=向上爬),A/D 过渡到"屏幕右的
		// 面内投影"。视线与面平行(FaceOn→0,平视)时完全沿用旧行为。
		const FVector CamRight = CameraPivot->GetRightVector();
		const FVector CamUp = CameraPivot->GetUpVector();
		const float FaceOn = FMath::Abs(FVector::DotProduct(Forward, Up));
		const float ScreenBlend = FMath::Clamp(
			(FaceOn - DriveBasisFaceOnMin) / FMath::Max(DriveBasisFaceOnMax - DriveBasisFaceOnMin, 0.01f),
			0.0f, 1.0f);

		if (FMath::Abs(Up.Z) < 0.5f)
		{
			// 墙面:平行端 = 视线水平分量当 W、竖直攀爬当 A/D(旧"左/右墙"符号约定);
			// 正对端 = 屏幕上方向当 W、屏幕右当 A/D。沿 FaceOn 混合,转相机时映射渐变
			// 而非突跳(用户要求"相机角度不同要有对应的不同操作形式")。
			// ⚠ 相机正对墙面(墙上跟随的常态!)时视线水平分量≈0(Along 退化),必须由
			// 屏幕相对端兜底——不能落回通用投影(那里同样是零向量,会退到球的前向)。
			FVector Along = FVector(Forward.X, Forward.Y, 0.0f);
			const bool bHaveAlong = Along.Normalize();
			const FVector Climb(0.0f, 0.0f,
				FVector::DotProduct(GravityDir, CamRight) >= 0.0f ? 1.0f : -1.0f);
			const FVector ScreenUp = (CamUp - Up * FVector::DotProduct(CamUp, Up)).GetSafeNormal();
			const FVector ScreenRight = (CamRight - Up * FVector::DotProduct(CamRight, Up)).GetSafeNormal();
			const bool bHaveScreen = !ScreenUp.IsNearlyZero() && !ScreenRight.IsNearlyZero();

			if (bHaveAlong && bHaveScreen)
			{
				Forward = (Along * (1.0f - ScreenBlend) + ScreenUp * ScreenBlend).GetSafeNormal();
				Right = (Climb * (1.0f - ScreenBlend) + ScreenRight * ScreenBlend).GetSafeNormal();
			}
			else if (bHaveScreen)
			{
				Forward = ScreenUp;
				Right = ScreenRight;
			}
			else if (bHaveAlong)
			{
				Forward = Along;
				Right = Climb;
			}
			// 都退化(极端视角):走下面的通用投影兜底。
		}
		else if (ScreenBlend > 0.0f)
		{
			// 地面/天花板:正对端改用"屏幕上方向在面内的投影"当 W(直视地面时视线投影
			// 近零,旧代码会退到球的前向、方向不可预期)。Right 恒为屏幕右的面内投影。
			const FVector UpOnPlane = (CamUp - Up * FVector::DotProduct(CamUp, Up)).GetSafeNormal();
			const FVector FwdParallel = (Forward - Up * FVector::DotProduct(Forward, Up)).GetSafeNormal();
			if (!UpOnPlane.IsNearlyZero())
			{
				Forward = FwdParallel.IsNearlyZero()
					? UpOnPlane
					: (FwdParallel * (1.0f - ScreenBlend) + UpOnPlane * ScreenBlend).GetSafeNormal();
			}
		}
	}
	else if (CameraPivot && FMath::Abs(Up.Z) < 0.5f)
	{
		// A/B 对照路径(bAdaptiveDriveBasis=false):旧墙上约定——W=沿墙横滚、A/D=攀爬
		// (A climbs on the screen-left wall, D climbs on the screen-right wall)。
		FVector Horizontal = FVector(Forward.X, Forward.Y, 0.0f);
		if (Horizontal.Normalize())
		{
			Forward = Horizontal;
			const float SideSign = FVector::DotProduct(GravityDir, CameraPivot->GetRightVector()) >= 0.0f ? 1.0f : -1.0f;
			Right = FVector(0.0, 0.0, SideSign);
		}
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
	// Debug:世界方向强制驱动(验证场景机制用;零向量 = 不介入,见头文件说明)。
	if (!DebugAutoDriveWorldDir.IsNearlyZero())
	{
		Desired = DebugAutoDriveWorldDir;
	}
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
		BallCollision->AddForce(Desired * DriveAccelerationCm * (bAiming ? AimDriveScale : 1.0f), NAME_None, true);
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
	// 鼠标视角诊断(2026-09-15 用户"想转向但死活不转"):把这条链路上的每个条件都打出来——
	// delta 本身 / 原生轮询开关 / 灵敏度倍率 / 输入模式。任何一项不对都能一眼定位。
	if (bFallbackCamDebugLog && GetWorld())
	{
		static double GSCamMouseLogNextTime = 0.0;
		const double NowT = GetWorld()->GetTimeSeconds();
		if (NowT >= GSCamMouseLogNextTime)
		{
			GSCamMouseLogNextTime = NowT + 0.4;
			UE_LOG(LogTemp, Log, TEXT("[GSCamMouse] t=%.2f mouse=(%.2f,%.2f) polling=%d sensMult=%.2f yawScale=%.3f pitchScale=%.3f sensScale=%.2f"),
				NowT, MouseX, MouseY, bEnableNativePollingInput ? 1 : 0, MouseSensitivityMultiplier,
				CameraYawDegreesPerMouseUnit, CameraPitchDegreesPerMouseUnit,
				(Camera && DefaultCameraFOV > 1.0f) ? FMath::Sqrt(Camera->FieldOfView / DefaultCameraFOV) : -1.0f);
		}
	}
	// Rail mode owns the camera; accumulated mouse offsets only apply to the
	// chase rig, otherwise they would suddenly apply on the next rail handoff.
		if (!RailCamera || !RailCamera->IsDriving())
		{
			if (!FMath::IsNearlyZero(MouseX) || !FMath::IsNearlyZero(MouseY))
			{
				// Pitch 不取反:BuildCameraRotation 里 CameraPitchDegrees 是正值=抬头,
				// 而 UE 的鼠标 delta Y 上抬为正,所以直接把 MouseY 加进去就是"鼠标上抬
				// → 相机上抬"。旧实现的 -MouseY 让俯仰和偏航反号,只有上下是反的。
				// 聚焦(ADS)时灵敏度按 FOV 比的开方衰减:有一点"聚焦变稳",
				// 但不会低到发木(全比例衰减实测太钝,用户反馈)。
				float SensScale = 1.0f;
				if (Camera && DefaultCameraFOV > 1.0f)
				{
					SensScale = FMath::Sqrt(Camera->FieldOfView / DefaultCameraFOV);
				}
				// 再乘用户倍率(存档里的设置)。这里是灵敏度的唯一相乘点。
				AddCameraLookInput(MouseX * CameraYawDegreesPerMouseUnit * SensScale * MouseSensitivityMultiplier,
					MouseY * CameraPitchDegreesPerMouseUnit * SensScale * MouseSensitivityMultiplier);
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

	// 新瞄准机制(RMB 瞄准 + LMB 翻转方块重力),取代旧的 G/1/2/3。
	UpdateAiming();

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
	// 无轨相机(新机制默认)时改调弹簧臂距离。
	const bool bTrailCloserDown = PC->IsInputKeyDown(TrailCloserKey);
	if (bTrailCloserDown && !bTrailCloserKeyWasDown)
	{
		if (RailCamera)
		{
			RailCamera->AdjustTrailDistance(-1.0f);
		}
		else
		{
			AdjustCameraDistance(-1.0f);
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
		else
		{
			AdjustCameraDistance(1.0f);
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
}

void AGSRollingBallPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 转向器过渡先于移动/相机推进:三者读到同一个平滑重力方向。
	UpdateGravityRedirect(DeltaSeconds);
	// 特殊滑梯吸附同理:先推进吸附(它自己驱动速度),再走移动/相机。
	UpdateFaceCapture(DeltaSeconds);

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
	// 航向绕"支撑上轴(不含俯仰)"旋转:上轴 ±Z 互换时同一鼠标动作给出的屏幕转向天然一致
	//(绕 -Z 转 +θ 等价于绕 +Z 转 -θ,恰好抵消旧实现的镜像)。
	// ⚠ 2026-09-15 关键修正:原来用 CurrentCameraUp,而 CurrentCameraUp 来自
	// `BuildCameraRotation(TargetCameraUp)` —— 该函数内部**总是**把 CameraPitchDegrees 计进总俯仰,
	// 所以 CurrentCameraUp 是**带俯仰的倾斜上轴**;绕倾斜轴转航向时水平方位只变化 cos(俯仰) 倍
	// (实测俯仰 70° → 每次 +90° 输入只转 21°,用户:"往右拽没用、只能往左")。必须用不含俯仰的
	// TargetCameraUp(=支撑面上方 / 未翻转时世界 up)。
	if (!CameraAimHeading.IsNearlyZero() && YawDeltaDegrees != 0.0f)
	{
		FVector Axis = TargetCameraUp.GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			Axis = CurrentCameraUp.GetSafeNormal();
		}
		if (!Axis.IsNearlyZero())
		{
			CameraAimHeading = FQuat(Axis, FMath::DegreesToRadians(YawDeltaDegrees))
				.RotateVector(CameraAimHeading);
		}
	}
	CameraPitchDegrees = FMath::Clamp(CameraPitchDegrees + PitchDeltaDegrees, -MaximumCameraPitchDegrees, MaximumCameraPitchDegrees);
}

void AGSRollingBallPawn::UpdateAiming()
{
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	const bool bAimDown = PC->IsInputKeyDown(AimKey) || bForceAimingDebug;
	if (bAimDown != bAiming)
	{
		bAiming = bAimDown;
		// 进/出聚焦的瞬间:记录非瞄准臂长基线(Q/E 随时改动都算数);
		// 解除瞄准时开 1.2s"快速回弹窗口",探针一离开天花板/墙就迅速放长。
		if (bAiming && CameraArm)
		{
			NonAimArmLengthCm = CameraArm->TargetArmLength;
		}
		if (!bAiming && GetWorld())
		{
			FastArmExtendUntilSeconds = GetWorld()->GetTimeSeconds() + 1.2f;
		}
		if (!bAiming && AimedBlock)
		{
			AimedBlock->SetAimHighlight(false);
			AimedBlock = nullptr;
		}
	}

	// TPS 聚焦(ADS):FOV 收窄 + 臂长贴近 + 越肩让位,全部平滑插值。
	// 仅无导轨相机时生效。臂长直接写 TargetArmLength 会被探针每帧覆盖——
	// 实际生效的拉近来自防穿墙探针(天花板/墙边自动收缩),这是设计行为;
	// 松开后的恢复速度由"快速回弹窗口"(FastArmExtendUntilSeconds)保证。
	if (Camera && CameraArm && (!RailCamera || !RailCamera->IsDriving()))
	{
		if (DefaultCameraFOV <= 1.0f)
		{
			DefaultCameraFOV = Camera->FieldOfView;
		}
		const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
		const float TargetFOV = bAiming ? AimTargetFOV : DefaultCameraFOV;
		const float TargetArm = bAiming ? AimArmLengthCm
			: (NonAimArmLengthCm > 1.0f ? NonAimArmLengthCm : CameraArm->TargetArmLength);
		const float TargetShoulder = bAiming ? AimShoulderOffsetCm : 0.0f;
		Camera->FieldOfView = FMath::FInterpTo(Camera->FieldOfView, TargetFOV, Dt, AimZoomSpeed);
		CameraArm->TargetArmLength = FMath::FInterpTo(CameraArm->TargetArmLength, TargetArm, Dt, AimZoomSpeed);
		FVector Socket = CameraArm->SocketOffset;
		Socket.Y = FMath::FInterpTo(Socket.Y, TargetShoulder, Dt, AimZoomSpeed);
		CameraArm->SocketOffset = Socket;
	}

	bAimKeyWasDown = bAimDown;
	if (!bAiming)
	{
		return;
	}

	// 屏幕中心射线 = 相机视线。
	FVector CamLoc = FVector::ZeroVector;
	FRotator CamRot = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSAimTrace), false);
	Params.AddIgnoredActor(this);
	const FVector End = CamLoc + CamRot.Vector() * AimRangeCm;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, Params);

	AGSBlockBase* NewAim = nullptr;
	if (bHit)
	{
		NewAim = Cast<AGSBlockBase>(Hit.GetActor());
		if (NewAim && !NewAim->CanChangeGravity())
		{
			NewAim = nullptr;
		}
	}

	if (NewAim != AimedBlock)
	{
		if (AimedBlock)
		{
			AimedBlock->SetAimHighlight(false);
		}
		AimedBlock = NewAim;
		if (AimedBlock)
		{
			AimedBlock->SetAimHighlight(true);
		}
	}

	// 锁定方块时左键 = 掉下来 ↔ 升起来。
	const bool bFireDown = PC->IsInputKeyDown(AimFireKey);
	if (bFireDown && !bAimFireKeyWasDown && AimedBlock)
	{
		const bool bNowRises = AimedBlock->ToggleGravityZ();
		if (bRedirectDebugLog)
		{
			UE_LOG(LogTemp, Log, TEXT("[GSAim] %s gravity -> %s"),
				*AimedBlock->GetName(), bNowRises ? TEXT("RISES(+Z)") : TEXT("FALLS(-Z)"));
		}
	}
	bAimFireKeyWasDown = bFireDown;
}

void AGSRollingBallPawn::AdjustCameraDistance(float DirectionSign)
{
	if (!CameraArm)
	{
		return;
	}
	// 与 Q/E 同一套步长;弹簧臂长变化经相机平滑链生效,不跳变。
	CameraArm->TargetArmLength = FMath::Clamp(
		CameraArm->TargetArmLength + DirectionSign * CameraDistanceStepCm, 220.0f, 900.0f);
	// 非瞄准态下 Q/E 的改动就是新的"恢复基线"。
	if (!bAiming)
	{
		NonAimArmLengthCm = CameraArm->TargetArmLength;
	}
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
