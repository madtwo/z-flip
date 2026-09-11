#include "GSGravityZones.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

#include "GSBlockBase.h"
#include "GSRollingBallPawn.h"

// ---------------------------------------------------------------------------------
// Zone manager
// ---------------------------------------------------------------------------------

AGSGravityZoneManager::AGSGravityZoneManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGSGravityZoneManager::BeginPlay()
{
	Super::BeginPlay();

	if (!bStartWithGravityDisabled)
	{
		return;
	}

	// 延到下一帧再禁用:Actor 的 BeginPlay 顺序不保证,方块自己的 BeginPlay 会用
	// 它序列化下来的 bAffectedByGravity 覆盖 GravityBody->bGravityEnabled。这时候
	// 立刻禁用会被后跑的方块 BeginPlay 顶掉(和 GSFramework 里 Manager 晚 spawn
	// 是同一类顺序坑)。下一帧计时器在所有 BeginPlay 之后跑,禁用才是终态。
	GetWorldTimerManager().SetTimerForNextTick(this, &AGSGravityZoneManager::DisableAllGravity);
}

void AGSGravityZoneManager::SetActiveZone(const TArray<AGSBlockBase*>& Blocks)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 目标区域先收成集合,禁用循环里跳过它们。不这么做的话"先全禁(带清零速度)再启用"
	// 会把正要激活的那批方块先定住一瞬——球在同一扇门前来回穿时,区域方块会一顿一顿。
	TSet<AGSBlockBase*> Keep;
	for (AGSBlockBase* Block : Blocks)
	{
		if (IsValid(Block))
		{
			Keep.Add(Block);
		}
	}

	for (TActorIterator<AGSBlockBase> It(World); It; ++It)
	{
		AGSBlockBase* Block = *It;
		if (Keep.Contains(Block))
		{
			Block->SetAffectedByGravity(true);
		}
		else
		{
			DisableBlockGravity(Block);
		}
	}
}

void AGSGravityZoneManager::DisableAllGravity()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AGSBlockBase> It(World); It; ++It)
	{
		DisableBlockGravity(*It);
	}
}

void AGSGravityZoneManager::DisableBlockGravity(AGSBlockBase* Block)
{
	Block->SetAffectedByGravity(false);
	Block->FreezeMotion();
}

void AGSGravityZoneManager::ResetAllZones()
{
	DisableAllGravity();
}

void AGSGravityZoneManager::OnResetWorld()
{
	ResetAllZones();
}

AGSGravityZoneManager* AGSGravityZoneManager::FindZoneManager(UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGSGravityZoneManager> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------------
// Detector
// ---------------------------------------------------------------------------------

AGSGravityDetector::AGSGravityDetector()
{
	PrimaryActorTick.bCanEverTick = false;

	TriggerPlane = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerPlane"));
	TriggerPlane->SetBoxExtent(TriggerExtent);
	// Trigger = 只产生重叠、不阻挡、不参与物理,小球直接穿过去。
	TriggerPlane->SetCollisionProfileName(TEXT("Trigger"));
	TriggerPlane->SetGenerateOverlapEvents(true);
	SetRootComponent(TriggerPlane);

	FlashMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlashMesh"));
	FlashMesh->SetupAttachment(TriggerPlane);
	FlashMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// 只动 bHiddenInGame,不动 bVisible:编辑器里留着可见好摆位 + 挂发光材质,
	// 游戏里由 PlayFlash/EndFlash 翻开翻回,两套开关不会互相打架。
	FlashMesh->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		FlashMesh->SetStaticMesh(CubeAsset.Object);
	}
}

void AGSGravityDetector::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (TriggerPlane)
	{
		TriggerPlane->SetBoxExtent(TriggerExtent);
	}
	if (FlashMesh)
	{
		// /Engine/BasicShapes/Cube 边长 100cm,extent 是半尺寸。
		FlashMesh->SetRelativeScale3D(TriggerExtent / 50.0f);
	}
}

void AGSGravityDetector::BeginPlay()
{
	Super::BeginPlay();

	if (TriggerPlane)
	{
		TriggerPlane->OnComponentBeginOverlap.AddDynamic(this, &AGSGravityDetector::HandleTriggerBeginOverlap);
	}
	if (FlashMesh)
	{
		FlashMesh->SetHiddenInGame(true);
	}
}

void AGSGravityDetector::HandleTriggerBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 1. 只响应小球
	AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(OtherActor);
	if (!Ball)
	{
		return;
	}

	// 2. 球速太慢不算"穿过"(停在触发面上蹭不该反复切区域)
	const FVector Velocity = Ball->GetBallLinearVelocity();
	if (Velocity.Size() < MinTriggerSpeed)
	{
		return;
	}

	// 3. 穿过方向
	const float Dot = FVector::DotProduct(Velocity.GetSafeNormal(), GetActorForwardVector());
	if (FMath::Abs(Dot) < MinDirectionalDot)
	{
		// 侧向擦过:分不出正反,不切区域
		return;
	}

	// 4. 选区域:朝前向穿过 → ZoneB,反向穿过 → ZoneA
	const TArray<AGSBlockBase*>* TargetZone = (Dot > 0.0f) ? &ZoneB_Blocks : &ZoneA_Blocks;

	// 5. 通知全局管理器
	if (AGSGravityZoneManager* Manager = AGSGravityZoneManager::FindZoneManager(GetWorld()))
	{
		Manager->SetActiveZone(*TargetZone);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[GSGravityDetector] %s 触发,但关卡里没有 AGSGravityZoneManager,方块重力不会切换。"),
			*GetName());
	}

	// 6. 闪光反馈
	PlayFlash();
}

void AGSGravityDetector::PlayFlash()
{
	if (!FlashMesh)
	{
		return;
	}

	FlashMesh->SetHiddenInGame(false);

	if (FlashDuration <= 0.0f)
	{
		EndFlash();
		return;
	}

	// 重新触发时重置计时,不叠加。
	GetWorldTimerManager().SetTimer(
		FlashTimerHandle, this, &AGSGravityDetector::EndFlash, FlashDuration, false);
}

void AGSGravityDetector::EndFlash()
{
	if (FlashMesh)
	{
		FlashMesh->SetHiddenInGame(true);
	}
}

void AGSGravityDetector::ResetDetector()
{
	GetWorldTimerManager().ClearTimer(FlashTimerHandle);
	EndFlash();
}
