// GravityShift - 单向空气墙实现

#include "GSOneWayBarrier.h"

#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GSRollingBallPawn.h"

AGSOneWayBarrier::AGSOneWayBarrier()
{
	PrimaryActorTick.bCanEverTick = true;

	Barrier = CreateDefaultSubobject<UBoxComponent>(TEXT("Barrier"));
	SetRootComponent(Barrier);
	Barrier->SetBoxExtent(FVector(200.0f, 20.0f, 150.0f));
	// **无碰撞**:真正的阻挡靠下面 Tick 里的速度钳制,这样才能做到"只挡一边"。
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrier->SetGenerateOverlapEvents(false);
	Barrier->SetHiddenInGame(true);
}

void AGSOneWayBarrier::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const FVector Dir = OutwardDirection.GetSafeNormal();
	if (Dir.IsNearlyZero())
	{
		return;
	}

	AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Ball)
	{
		return;
	}
	// **滑行/吸附期间绝对不拦**(2026-09-17 用户:"不要耽误用滑梯改变重力后在平台上爬"):
	// 那两种状态下球的速度由 Pawn 逐帧强制,空气墙在这儿抢速度会把滑梯弄坏。
	// 滑行结束后照常接管(球已经在外面了,只挡它继续往外)。
	if (Ball->IsGravityRedirecting() || Ball->IsFaceCapturing())
	{
		return;
	}
	USphereComponent* Sphere = Ball->GetBallCollisionComponent();
	if (!Sphere || !Sphere->IsSimulatingPhysics())
	{
		return;
	}

	// 球心是否落在"盒子 + 厚度"内(在本组件空间里判断,盒子怎么转/缩放都行)。
	const FVector Local = GetActorTransform().InverseTransformPosition(Sphere->GetComponentLocation());
	const FVector Extent = Barrier->GetScaledBoxExtent() + FVector(BarrierThicknessCm);
	if (FMath::Abs(Local.X) > Extent.X || FMath::Abs(Local.Y) > Extent.Y || FMath::Abs(Local.Z) > Extent.Z)
	{
		return;
	}

	const float Radius = Sphere->GetScaledSphereRadius();
	const FVector BallLoc = Sphere->GetComponentLocation();
	const float CoordAlongDir = FVector::DotProduct(BallLoc - GetActorLocation(), Dir);
	// 盒子在"外向"上的半尺寸(三个轴全按外向取,斜向时偏保守,只会挡得更早不会漏)。
	const float ExtentAlongDir = FMath::Abs(Dir.X) * Extent.X + FMath::Abs(Dir.Y) * Extent.Y + FMath::Abs(Dir.Z) * Extent.Z;
	// 球心允许到达的最外位置:球面正好贴住盒子外侧面。
	const float StopCoord = ExtentAlongDir - Radius;

	const FVector Velocity = Sphere->GetPhysicsLinearVelocity();
	const float OutwardSpeed = FVector::DotProduct(Velocity, Dir);

	bool bBlocked = false;
	// ① **位置钳制(硬墙)** —— 这条是关键:只削速度挡不住驱动力。
	//    驱动力在物理步里每帧重新给出速度(实测约 60cm/s 的稳定"蹭行"),球会一路蹭穿 130cm 厚的墙。
	//    把球心的外向坐标钉在 StopCoord 以内,驱动力再大也过不去。
	//    ⚠ 但**明显在往内走时不钳**(否则从外面进来的球会被硬拽到停止线上 = 单向变成双向):
	//    这正是"外面进得来"那一半。
	const bool bMovingInward = OutwardSpeed < -1.0f;
	// 只在"停止线外一点点"(MaxSnapCm 内)才拽回:从很外面掉进来的球不会被硬拉一根大位移。
	const float Overshoot = CoordAlongDir - StopCoord;
	if (!bMovingInward && Overshoot > 0.0f && Overshoot <= MaxSnapCm)
	{
		Sphere->SetWorldLocation(BallLoc - Dir * Overshoot, false, nullptr, ETeleportType::TeleportPhysics);
		bBlocked = true;
	}
	// ② 外向速度清零(让"贴着外侧面"这件事不抖:速度不为正就不会被解算器往外顶)。
	if (OutwardSpeed > 0.0f)
	{
		FVector NewVelocity = Velocity - Dir * OutwardSpeed;
		if (PushBackCm > 0.0f)
		{
			NewVelocity -= Dir * PushBackCm;
		}
		Sphere->SetPhysicsLinearVelocity(NewVelocity);
		bBlocked = true;
	}
	if (!bBlocked)
	{
		// 在往内走 / 横穿 → 空气墙不拦,一点不影响玩家。
		return;
	}

	if (bDebugLog)
	{
		// 节流到每 0.2s 一条:玩家顶着墙推时不会刷屏,但"到底有没有拦"一目了然。
		const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
		if (Now - LastBlockLogTime >= 0.2)
		{
			LastBlockLogTime = Now;
			UE_LOG(LogTemp, Log, TEXT("[GSBarrier] %s 拦住球: 外向速度 %.0f cm/s, 越界 %.0f cm (外向=%s)"),
				*GetNameSafe(this), OutwardSpeed, FMath::Max(CoordAlongDir - StopCoord, 0.0f), *Dir.ToString());
		}
	}
}
