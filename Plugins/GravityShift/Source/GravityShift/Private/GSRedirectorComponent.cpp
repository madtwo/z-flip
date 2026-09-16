// GravityShift v7 - redirector chute: touching it slides the ball onto the connected wall
// and rotates gravity to that wall (replaces the direct G-key flip for level design).

#include "GSRedirectorComponent.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GSLandingResponseComponent.h"
#include "GSRollingBallPawn.h"

UGSRedirectorComponent::UGSRedirectorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UGSRedirectorComponent::BeginPlay()
{
	Super::BeginPlay();

	// 触发范围 = 自身(网格)包围盒外扩(廉价先筛),同时记住网格原始包围盒(碰到判定用)。
	if (const AActor* Owner = GetOwner())
	{
		MeshBounds = Owner->GetComponentsBoundingBox(true);
		TriggerBounds = MeshBounds.ExpandBy(TriggerInflateCm);
	}

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s armed: faces=%s<->%s bounds=(%s)..(%s)"),
			*GetNameSafe(GetOwner()),
			*GSGravity::GetDirectionDisplayName(GravityDirectionA),
			*GSGravity::GetDirectionDisplayName(GravityDirectionB),
			*TriggerBounds.Min.ToString(), *TriggerBounds.Max.ToString());
	}
}

bool UGSRedirectorComponent::IsBallTouchingChute(const USphereComponent& BallSphere, const FVector& EntryUp, const FVector& ExitUp, FVector& OutNormal) const
{
	OutNormal = FVector::ZeroVector;
	const UWorld* World = GetWorld();
	if (!World || !MeshBounds.IsValid)
	{
		return false;
	}

	const FVector BallLoc = BallSphere.GetComponentLocation();
	const float Radius = BallSphere.GetScaledSphereRadius();
	const float Reach = FMath::Max(Radius + ContactTouchMarginCm, 1.0f);

	// 多方向线探针取"最近命中":朝网格包围盒最近点 / 入口"下" / 出口"下"。
	// 用线而不是球面扫掠:球静止在地面时扫掠起点已与地面重叠,扫掠会立刻返回地面
	// (实测踩过);线从球心出发不与地面重叠,能真正打到滑梯的复杂碰撞三角面。
	FVector Dirs[3];
	int32 DirCount = 0;
	const FVector ToBox = MeshBounds.GetClosestPointTo(BallLoc) - BallLoc;
	if (ToBox.SizeSquared() > 1.0f)
	{
		Dirs[DirCount++] = ToBox.GetSafeNormal();
	}
	Dirs[DirCount++] = (-EntryUp).GetSafeNormal();
	Dirs[DirCount++] = (-ExitUp).GetSafeNormal();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSRedirectContact), false, BallSphere.GetOwner());
	Params.AddIgnoredComponent(&BallSphere);
	bool bHitChute = false;
	float BestDist = TNumericLimits<float>::Max();
	for (int32 i = 0; i < DirCount; ++i)
	{
		if (Dirs[i].IsNearlyZero())
		{
			continue;
		}
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, BallLoc, BallLoc + Dirs[i] * Reach, ECC_WorldStatic, Params))
		{
			if (Hit.GetActor() == GetOwner() && Hit.Distance <= Reach && Hit.Distance < BestDist)
			{
				bHitChute = true;
				BestDist = Hit.Distance;
				OutNormal = Hit.ImpactNormal.GetSafeNormal();
			}
		}
	}
	return bHitChute;
}

// 特殊滑梯:面吸附触发(2026-09-15 用户需求)。
// 判定链:两面垂直 → 球当前重力落在某一面 → 该面允许吸附进入 → 球心朝该面法线反向探到
// **本滑梯** → 接触法线确实是这一面的 → (可选)球在沿面前进 → 速度达下限 → 交给 Pawn。
// 关键区别:旧逻辑的 gate[side] 把"撞在竖直面上"整类拒掉,而面吸附要接的正是这一类。
bool UGSRedirectorComponent::TryBeginFaceCapture(AGSRollingBallPawn& Ball, const USphereComponent& BallSphere,
	const FVector& BallLoc, const FVector& Velocity, float Speed)
{
	if (Ball.IsFaceCapturing() || Ball.IsFaceCaptureCoolingDown())
	{
		// 正在吸附中 / 刚刚释放(见 Pawn 的 FaceCaptureReleaseCooldownSeconds):
		// 后者是双向的必需品——A→B 刚把球送上墙的那一帧,球还贴着墙、重力已是墙的重力,
		// 本滑梯是三个件叠着摆的,另外两个件的触发盒同样罩着这颗球,不设冷却就会被它们
		// 立刻反向吸回高台(来回弹)。冷却期内球已沿墙走开,探针打不到圆弧了。
		return false;
	}
	// 支撑门(复用同一批开关):空中擦过竖直面的球不吸附。
	if (bRequireSupportToTrigger && Ball.LandingResponse)
	{
		if (!Ball.LandingResponse->IsSupported()
			|| Ball.LandingResponse->GetAirborneSeconds() > MaxAirborneSecondsForTrigger)
		{
			return false;
		}
	}
	if (Speed < MinTriggerSpeedCm)
	{
		return false;
	}

	const FVector DirA = GSGravity::DirectionToVector(GravityDirectionA).GetSafeNormal();
	const FVector DirB = GSGravity::DirectionToVector(GravityDirectionB).GetSafeNormal();
	if (FMath::Abs(FVector::DotProduct(DirA, DirB)) > 0.1f)
	{
		return false;
	}

	// 球当前重力落在哪一面:入口面就是它"骑着的"那一面。
	const FVector BallGravity = Ball.GetActiveGravityDirection().GetSafeNormal();
	const float DotA = FVector::DotProduct(BallGravity, DirA);
	const float DotB = FVector::DotProduct(BallGravity, DirB);
	const bool bEnterFromA = DotA >= DotB;
	if (FVector::DotProduct(BallGravity, bEnterFromA ? DirA : DirB) < EntryFaceGravityMin)
	{
		return false;
	}
	if (!(bEnterFromA ? bCaptureEntryFromA : bCaptureEntryFromB))
	{
		return false;
	}

	// 出口面 = 另一面。入口 B(竖直面)时出口 A(平面)= 竖直向下:就是"转成重力向下"。
	const FVector EntryGravity = bEnterFromA ? DirA : DirB;
	const FVector ExitGravity = bEnterFromA ? DirB : DirA;
	const FVector EntryFaceNormal = -EntryGravity;
	const FVector ExitFaceNormal = -ExitGravity;

	// 朝入口面法线的反向探一下,取回真实接触法线(必须命中的是本滑梯)。
	const float Radius = BallSphere.GetScaledSphereRadius();
	const float Reach = FMath::Max(Radius + ContactTouchMarginCm, 1.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GSFaceCaptureEntry), false, BallSphere.GetOwner());
	Params.AddIgnoredComponent(&BallSphere);
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, BallLoc, BallLoc - EntryFaceNormal * Reach,
		ECC_WorldStatic, Params))
	{
		return false;
	}
	if (Hit.GetActor() != GetOwner())
	{
		return false;
	}

	const FVector ContactNormal = Hit.Normal.GetSafeNormal();
	if (FVector::DotProduct(ContactNormal, EntryFaceNormal) < FaceCaptureEntryNormalMin)
	{
		// 贴的不是这一面(例如从平面那侧擦过) → 交给原弯道逻辑。
		return false;
	}

	// 沿面前进方向 = 旋转轴 × 接触法线(入口竖直面时朝上,出口平面时朝外)。
	// 这个方向必然是"把接触法线往出口面转"的那一侧(轴就是这么构造的),不存在方向二义性;
	// 反着滚的球由下面的方向门拒掉。
	const FVector Axis = FVector::CrossProduct(EntryFaceNormal, ExitFaceNormal).GetSafeNormal();
	const FVector ApproachDir = FVector::CrossProduct(Axis, ContactNormal).GetSafeNormal();
	if (ApproachDir.IsNearlyZero())
	{
		return false;
	}

	// 吸附方式由**入口面**决定(用户 2026-09-16 "看人下菜"的落点):
	//   · 入口 = B(竖直面):球是"掉下去弹起来擦到墙上"或"已经骑在墙上往上滚"——上一轮
	//     已验收的路径,保持原硬吸附(定速沿面驱动),手感不动。
	//   · 入口 = A(平面/高台):球是自己在地面上滚到圆弧的,走温和吸附——保留球自己的
	//     切向速度,只切掉"离开面"的法向分量,靠重力随路程旋转把它贴到墙上。
	const bool bGroundedEntry = bEnterFromA;
	const float MinApproachSpeedCm = bGroundedEntry
		? FaceCaptureGroundMinApproachSpeedCm
		: FaceCaptureMinApproachSpeedCm;
	const float ApproachSpeedCm = FVector::DotProduct(Velocity, ApproachDir);
	if (MinApproachSpeedCm > 0.0f && ApproachSpeedCm < MinApproachSpeedCm)
	{
		if (bDebugLog)
		{
			UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s face-capture rejected(%s): approach=%.0f < %.0f (球不是朝圆弧滚)"),
				*GetNameSafe(GetOwner()), bGroundedEntry ? TEXT("A/high-platform") : TEXT("B/wall"),
				ApproachSpeedCm, MinApproachSpeedCm);
		}
		return false;
	}

	Ball.BeginFaceCapture(GetOwner(), ContactNormal, ExitGravity, FaceCaptureSpeedCm,
		FaceCaptureStickAccelCm, FaceCaptureExitNormalDot,
		bGroundedEntry, FaceCaptureGroundMinSpeedCm, FaceCaptureGroundMaxSpeedCm);

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s face-capture: style=%s ball=(%.0f,%.0f,%.0f) n=(%.2f,%.2f,%.2f) %s→%s v=(%.0f,%.0f,%.0f) approach=%.0f"),
			*GetNameSafe(GetOwner()), bGroundedEntry ? TEXT("grounded") : TEXT("hard"),
			BallLoc.X, BallLoc.Y, BallLoc.Z,
			ContactNormal.X, ContactNormal.Y, ContactNormal.Z,
			*GSGravity::GetDirectionDisplayName(bEnterFromA ? GravityDirectionA : GravityDirectionB),
			*GSGravity::GetDirectionDisplayName(bEnterFromA ? GravityDirectionB : GravityDirectionA),
			Velocity.X, Velocity.Y, Velocity.Z, ApproachSpeedCm);
	}
	LastFireTime = GetWorld()->GetTimeSeconds();
	return true;
}

void UGSRedirectorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !TriggerBounds.IsValid)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const double Now = World->GetTimeSeconds();
	AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Ball)
	{
		return;
	}

	const USphereComponent* BallSphere = Ball->GetBallCollisionComponent();
	if (!BallSphere)
	{
		return;
	}
	const FVector BallLoc = BallSphere->GetComponentLocation();

	// 心跳(调试):每 30 帧一次,确认 tick 在跑 + 球是否在触发盒内。
	if (bDebugLog && (GFrameCounter % 30 == 0))
	{
		UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s tick ball=(%.0f,%.0f,%.0f) inBox=%d riding=%d"),
			*GetNameSafe(GetOwner()), BallLoc.X, BallLoc.Y, BallLoc.Z,
			TriggerBounds.IsInside(BallLoc) ? 1 : 0, bRiding ? 1 : 0);
	}

	// ---- 滑行中:保持,直到球离开触发盒**且重力旋转已走完**才释放 ----
	// (旋转没走完就释放会留下一个介于两个面之间的重力,看着像"没转过去")
	if (bRiding)
	{
		if (!Ball->IsGravityRedirecting())
		{
			bRiding = false;
			LastFireTime = Now;
		}
		else if (!TriggerBounds.IsInside(BallLoc) && Ball->GetGravityRedirectProgress() >= 1.0f)
		{
			Ball->EndGravityRedirect();
			bRiding = false;
			LastFireTime = Now;
			if (bDebugLog)
			{
				UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s released: ball=(%.0f,%.0f,%.0f)"),
					*GetNameSafe(GetOwner()), BallLoc.X, BallLoc.Y, BallLoc.Z);
			}
		}
		return;
	}

	// 别的转向器正在滑行 / 冷却中 → 不参与。
	if (Ball->IsGravityRedirecting())
	{
		return;
	}
	if (LastFireTime >= 0.0 && Now - LastFireTime < CooldownSeconds)
	{
		return;
	}

	if (!TriggerBounds.IsInside(BallLoc))
	{
		return;
	}

	const FVector Velocity = Ball->GetBallLinearVelocity();
	const float Speed = Velocity.Size();

	// ---- 特殊滑梯:面吸附(只对勾了 bFaceCaptureMode 的滑梯生效)----
	// 放在弯道判定之前:球"从侧面碰到竖直面"这一下,旧逻辑会走 gate[side] 直接拒掉,
	// 面吸附模式要接住的正是这一下。命中后由 Pawn 逐帧驱动到另一面并提交重力。
	if (bFaceCaptureMode)
	{
		if (TryBeginFaceCapture(*Ball, *BallSphere, BallLoc, Velocity, Speed))
		{
			return;
		}
		if (!bFaceCaptureAlsoClassic)
		{
			// 没吸附上(例如从平面那侧滚过)就什么都不做:否则吸附刚把球送到另一面上,
			// 旧弯道逻辑会在同一帧又把它甩回去。
			return;
		}
	}

	// 调试:球在触发盒内时,把各门限的实测值逐帧打出来(定位"为什么不触发")。
	const bool bGateDebug = bDebugLog;
	auto GateLog = [&](const TCHAR* Stage, const FVector& ExitDirForLog, float Align, bool bTouch)
	{
		if (bGateDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s gate[%s] ball=(%.0f,%.0f,%.0f) v=(%.0f,%.0f,%.0f) speed=%.0f exit=%s align=%.2f touch=%d"),
				*GetNameSafe(GetOwner()), Stage, BallLoc.X, BallLoc.Y, BallLoc.Z,
				Velocity.X, Velocity.Y, Velocity.Z, Speed,
				*ExitDirForLog.ToString(), Align, bTouch ? 1 : 0);
		}
	};

	// ---- 双向:由球当前重力识别入口面(A 或 B),出口就是另一面 ----
	const FVector DirA = GSGravity::DirectionToVector(GravityDirectionA).GetSafeNormal();
	const FVector DirB = GSGravity::DirectionToVector(GravityDirectionB).GetSafeNormal();
	// 两面必须互相垂直(90° 弯道),否则构不成转向器。
	if (FMath::Abs(FVector::DotProduct(DirA, DirB)) > 0.1f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[GSRedirector] %s 的两个面(%s/%s)不垂直,无法构成 90° 弯道"),
			*GetNameSafe(GetOwner()),
			*GSGravity::GetDirectionDisplayName(GravityDirectionA),
			*GSGravity::GetDirectionDisplayName(GravityDirectionB));
		return;
	}

	const FVector BallGravity = Ball->GetActiveGravityDirection().GetSafeNormal();
	const float DotA = FVector::DotProduct(BallGravity, DirA);
	const float DotB = FVector::DotProduct(BallGravity, DirB);
	const bool bEnterFromA = DotA >= DotB;
	const FVector EntryGravity = bEnterFromA ? DirA : DirB;
	const FVector ExitDir = bEnterFromA ? DirB : DirA;

	// 球必须确实在其中一个面上(在 90° 弯道里不存在第三种重力)。
	if (FVector::DotProduct(BallGravity, EntryGravity) < EntryFaceGravityMin)
	{
		GateLog(TEXT("face"), ExitDir, 0.0f, false);
		return;
	}

	// 单面进入开关(2026-09-13):默认两面都开(双向);关了的那一面进入时直接拒绝。
	if (bEnterFromA ? !bAllowEntryFromA : !bAllowEntryFromB)
	{
		GateLog(TEXT("entryside"), ExitDir, 0.0f, false);
		return;
	}

	const FVector EntryUp = -EntryGravity;
	const FVector ExitUp = -ExitDir;
	const FVector BendAxis = FVector::CrossProduct(EntryUp, ExitUp).GetSafeNormal();
	if (BendAxis.IsNearlyZero())
	{
		return;
	}

	// 是否"碰到圆弧",并取回接触面法线(碰住滑梯才转重力;反向进入时球常被顶停,速度掉到几十)。
	FVector ContactNormal = FVector::ZeroVector;
	const bool bTouch = IsBallTouchingChute(*BallSphere, EntryUp, ExitUp, ContactNormal);
	if (!bTouch)
	{
		GateLog(TEXT("touch"), ExitDir, 0.0f, false);
		return;
	}

	// **撞侧面不触发**:接触面法线沿"滑梯宽度方向"(弯道轴) → 撞的是滑梯侧壁(平直面),
	// 不是正面圆弧。用户要求:撞侧面不要动,只有正面圆弧接地那块才触发。
	const float NormalAxisDot = FMath::Abs(FVector::DotProduct(ContactNormal, BendAxis));
	if (NormalAxisDot > MaxLateralNormalDot)
	{
		GateLog(TEXT("side"), ExitDir, NormalAxisDot, true);
		return;
	}

	// **只有碰到"正面接地那一块"才触发**(用户要求):球心到入口侧面(地面/墙/天花板
	// 那一侧的面)的距离 ≤ 球半径 + EntryLipBandCm 才算碰在圆弧接地处;碰在滑梯顶面/
	// 背面等其它位置不触发。入口侧面 = 网格包围盒沿入口重力轴**离球最近的那一侧**面。
	// 2026-09-15:原先硬取"入口重力指向的那一侧"(Center + Sign(N)*Extent),只对
	// "滑梯坐在球的支撑面上"那种摆法成立(球贴包围盒底面,如 LDI_Gravityshift);
	// 弯道往反方向卷时(如平台外圆角 Blockout_Corner_Curved:球骑在包围盒**顶面**,
	// 弧往下卷)会取到对面那侧 → 实测 DistToLip=276 ≫ R+60=110,正常触发全被
	// gate[lip] 拒掉。改成按球实际在哪一侧选面,两种摆法都对,原摆法结果不变。
	const float SignedCm = FVector::DotProduct(BallLoc - MeshBounds.GetCenter(), EntryGravity);
	const float ExtentAlongGravityCm = FMath::Abs(FVector::DotProduct(MeshBounds.GetExtent(), EntryGravity));
	const float DistToLipCm = FMath::Abs(FMath::Abs(SignedCm) - ExtentAlongGravityCm);
	const float Radius = BallSphere->GetScaledSphereRadius();
	if (DistToLipCm > Radius + EntryLipBandCm)
	{
		GateLog(TEXT("lip"), ExitDir, DistToLipCm, true);
		return;
	}

	// **"真的骑在面上"加固(2026-09-13 用户反馈)**:擦过/弹开/空中掠过不应触发。
	// ①支撑门:球必须在支撑态且悬空时长 ≤ MaxAirborneSecondsForTrigger——滑地/滑墙
	//   进入的球"骑在面上";从墙沿掉下来、空中飞过时擦到滑梯的球则是悬空的。
	// ②分离门:球相对接触面的速度不能朝"离开滑梯"方向过大(刚被边缘弹开的球,速度
	//   沿接触法线朝外)。用户误触发那次球是悬空脱离墙面的:vIn 含 +341cm/s 离面分量。
	if (bRequireSupportToTrigger && Ball->LandingResponse)
	{
		const float AirborneSec = Ball->LandingResponse->GetAirborneSeconds();
		if (!Ball->LandingResponse->IsSupported() || AirborneSec > MaxAirborneSecondsForTrigger)
		{
			GateLog(TEXT("airborne"), ExitDir, AirborneSec, bTouch);
			return;
		}
	}
	if (bRejectSeparatingContact)
	{
		const float SepSpeedCm = FVector::DotProduct(Velocity, ContactNormal);
		if (SepSpeedCm > SeparationRejectSpeedCm)
		{
			GateLog(TEXT("separating"), ExitDir, SepSpeedCm, bTouch);
			return;
		}
	}

	// 静止球不触发。
	if (Speed < MinTriggerSpeedCm)
	{
		GateLog(TEXT("speed"), ExitDir, 0.0f, true);
		return;
	}

	// 方向判定:速度必须大体朝出口方向(入口行进方向 = 出口重力方向,正反通用);
	// 只有"几乎停住"的球(速度 < SlowEntrySpeedCm,反向下滑常被滑梯顶停)才豁免,
	// 否则"侧面撞进来 / 在滑梯里弹跳"的球也会被当成进入。
	const float Align = Speed > 1.0f ? FVector::DotProduct(Velocity / Speed, ExitDir) : 0.0f;
	if (Speed >= SlowEntrySpeedCm && Align < EntryAlignmentMin)
	{
		GateLog(TEXT("align"), ExitDir, Align, true);
		return;
	}

	// 入口速度整理:侧向(垂直弯道平面)分量按比例清掉;前向速度作为滑行速度
	// (太快截到上限,太慢补到保底值)。
	FVector Tangential = Velocity;
	if (LateralKillFraction > 0.0f)
	{
		const float Lateral = FVector::DotProduct(Tangential, BendAxis);
		Tangential -= BendAxis * (Lateral * LateralKillFraction);
	}
	float ForwardSpeed = FVector::DotProduct(Tangential, ExitDir);
	ForwardSpeed = FMath::Clamp(ForwardSpeed, MinEntrySpeedCm, MaxRideSpeedCm);

	// 交给 Pawn 滑行(速度在 Pawn 里逐帧锁在弯道切向;这里先摆好入口速度)。
	Ball->SetBallLinearVelocity(ExitDir * ForwardSpeed, false);
	Ball->BeginGravityRedirect(ExitDir, ForwardSpeed, BendAxis, RidePathLengthCm);
	bRiding = Ball->IsGravityRedirecting();
	LastFireTime = Now;

	if (bDebugLog)
	{
		UE_LOG(LogTemp, Log, TEXT("[GSRedirector] %s fired: ball=(%.0f,%.0f,%.0f) vIn=(%.0f,%.0f,%.0f) %s→%s axis=(%.2f,%.2f,%.2f) ride=%.0f path=%.0f"),
			*GetNameSafe(GetOwner()), BallLoc.X, BallLoc.Y, BallLoc.Z,
			Velocity.X, Velocity.Y, Velocity.Z,
			*GSGravity::GetDirectionDisplayName(bEnterFromA ? GravityDirectionA : GravityDirectionB),
			*GSGravity::GetDirectionDisplayName(bEnterFromA ? GravityDirectionB : GravityDirectionA),
			BendAxis.X, BendAxis.Y, BendAxis.Z, ForwardSpeed, RidePathLengthCm);
	}
}
