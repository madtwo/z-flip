#include "GSBlockBase.h"

#include "Components/StaticMeshComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

#include "GSBreakableComponent.h"
#include "GSGravityBodyComponent.h"
#include "GSGridSnapComponent.h"
#include "GSProfiles.h"
#include "GSResettableComponent.h"
#include "GSSurfaceReceiverComponent.h"

AGSBlockBase::AGSBlockBase()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BlockMesh"));
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CubeAsset.Object);
	}

	// 准星瞄准的"边缘发光"覆盖材质(由安装脚本生成在插件 Content)。
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RimGlowAsset(TEXT("/GravityShift/Materials/M_GS_RimGlow.M_GS_RimGlow"));
	if (RimGlowAsset.Succeeded())
	{
		AimGlowMaterial = RimGlowAsset.Object;
	}

	GravityBody = CreateDefaultSubobject<UGSGravityBodyComponent>(TEXT("GravityBody"));
	SurfaceReceiver = CreateDefaultSubobject<UGSSurfaceReceiverComponent>(TEXT("SurfaceReceiver"));
	BreakableComponent = CreateDefaultSubobject<UGSBreakableComponent>(TEXT("Breakable"));
	Resettable = CreateDefaultSubobject<UGSResettableComponent>(TEXT("Resettable"));
	GridSnapComponent = CreateDefaultSubobject<UGSGridSnapComponent>(TEXT("GridSnap"));

	// Snap is opt-in per block profile. Off by default so existing (non-grid-aligned)
	// levels are unaffected; grid levels enable it on the BlockProfile.
	GridSnapComponent->SetSnapEnabled(false);

	// Tick 默认关,只有"玩家推不动"的方块在 BeginPlay 里打开(见 bImmovableByPlayer)。
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AGSBlockBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 完全推不动:方块只该沿**自己的重力轴**动(±Z 切换/升起来就是沿轴走),而玩家球撞上来的
	// 是垂直于轴的方向。垂直分量每帧清零 —— 那一帧的横向速度留不到下一帧,玩家怎么撞都推不动。
	// (只靠 ImmovableMassKg=2000kg 时,高速球仍能把方块顶出几厘米;这道是硬保障。)
	if (!bImmovableByPlayer || !Mesh || !Mesh->IsSimulatingPhysics())
	{
		return;
	}
	const FVector Axis = GetGravityAxisWorld().GetSafeNormal();
	if (Axis.IsNearlyZero())
	{
		return;
	}
	const FVector Velocity = Mesh->GetPhysicsLinearVelocity();
	const FVector AlongAxis = Axis * FVector::DotProduct(Velocity, Axis);
	if (!(Velocity - AlongAxis).IsNearlyZero(0.5f))
	{
		Mesh->SetPhysicsLinearVelocity(AlongAxis);
	}
}

void AGSBlockBase::BeginPlay()
{
	Super::BeginPlay();

	if (BlockProfile)
	{
		ApplyBlockProfile(BlockProfile);
	}
	else
	{
		ApplyCurrentConfiguration();
	}

	if (GravityBody)
	{
		GravityBody->RefreshReferences();
	}

	// "玩家推不动"的方块才开 Tick(每帧清横向速度,见 Tick)。
	PrimaryActorTick.bStartWithTickEnabled = false;
	SetActorTickEnabled(bImmovableByPlayer);
}

void AGSBlockBase::ApplyBlockProfile(UGSBlockProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	BlockProfile = NewProfile;
	bStartSimulatingPhysics = NewProfile->bStartSimulatingPhysics;
	bAffectedByGravity = NewProfile->bAffectedByGravity;
	bCanBreakTargets = NewProfile->bCanBreakTargets;
	bBreakable = NewProfile->bBreakable;
	bUseContinuousCollisionDetection = NewProfile->bUseContinuousCollisionDetection;
	GravityScale = NewProfile->GravityScale;
	MassOverrideKg = NewProfile->MassOverrideKg;
	MaximumSpeedCm = NewProfile->MaximumSpeedCm;
	ImpactEnergyMultiplier = NewProfile->ImpactEnergyMultiplier;
	ImpactSourceTag = NewProfile->ImpactSourceTag;

	if (GridSnapComponent)
	{
		GridSnapComponent->SetSnapEnabled(NewProfile->bSnapToGrid);
	}

	if (NewProfile->Mesh)
	{
		SetBlockMesh(NewProfile->Mesh);
	}

	if (BreakableComponent)
	{
		BreakableComponent->ApplyBreakProfile(NewProfile->BreakProfile);
	}

	ApplyCurrentConfiguration();
}

void AGSBlockBase::SetBlockMesh(UStaticMesh* NewMesh)
{
	if (Mesh && NewMesh)
	{
		Mesh->SetStaticMesh(NewMesh);
	}
}

void AGSBlockBase::SetSimulatingPhysics(bool bSimulate)
{
	bStartSimulatingPhysics = bSimulate;
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(bSimulate);
		Mesh->SetEnableGravity(false);
	}
}

void AGSBlockBase::SetAffectedByGravity(bool bAffected)
{
	bAffectedByGravity = bAffected;
	if (GravityBody)
	{
		GravityBody->SetGravityEnabled(bAffected);
	}
}

void AGSBlockBase::FreezeMotion()
{
	if (!Mesh)
	{
		return;
	}

	Mesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
}

bool AGSBlockBase::CanChangeGravity() const
{
	// 悬浮待命方块:重力尚未"开闸"(bGravityEnabled=false),但准星必须能锁定它——
	// 首次点击后由 ToggleGravityZ 唤醒。
	return GravityBody && Mesh && Mesh->IsSimulatingPhysics()
		&& (GravityBody->bGravityEnabled || (bStartStaticUntilClicked && !bActivatedByClick));
}

FVector AGSBlockBase::GetGravityAxisWorld() const
{
	// 零轴哨兵 = 老行为(世界 ±Z)。没配过 GravityAxisLocal 的方块(哪怕被旋转过)
	// 逐字节等价于改动前,不会被本特性误伤。
	if (GravityAxisLocal.IsNearlyZero())
	{
		return FVector(0.0, 0.0, bGravityRises ? 1.0 : -1.0);
	}
	return GetActorTransform().TransformVectorNoScale(GravityAxisLocal.GetSafeNormal())
		* (bGravityRises ? 1.0 : -1.0);
}

void AGSBlockBase::SetGravityRises(bool bRises)
{
	bGravityRises = bRises;
	if (GravityBody)
	{
		GravityBody->SetOwnGravityDirection(GetGravityAxisWorld(), true);
	}
}

bool AGSBlockBase::ToggleGravityZ()
{
	SetGravityRises(!bGravityRises);

	// 悬浮待命方块:第一次点击 = 唤醒。开重力(含把配置也改成"受重力",防止后续
	// ApplyCurrentConfiguration 又把它关掉)、恢复缩放、唤醒刚体,立刻沿新方向走。
	if (bStartStaticUntilClicked && !bActivatedByClick)
	{
		bActivatedByClick = true;
		bAffectedByGravity = true;
		SetAffectedByGravity(true);
		if (GravityBody)
		{
			GravityBody->GravityScale = GravityScale;
		}
		if (Mesh)
		{
			Mesh->WakeAllRigidBodies();
		}
	}
	return bGravityRises;
}

void AGSBlockBase::SetAimHighlight(bool bOn)
{
	if (!Mesh || bOn == bAimHighlightOn)
	{
		return;
	}
	// 用 Overlay 叠加材质而不是换槽位:方块原材质完全保留,只有边缘微微发光
	// (v1 换槽位会把方块本体变成黑色半透明,用户不要)。
	Mesh->SetOverlayMaterial(bOn ? AimGlowMaterial.Get() : nullptr);
	bAimHighlightOn = bOn;
}

void AGSBlockBase::SetCanBreakTargets(bool bCanBreak)
{
	bCanBreakTargets = bCanBreak;
	if (GravityBody)
	{
		GravityBody->bCanBreakTargets = bCanBreak;
	}
}

void AGSBlockBase::SetBreakable(bool bIsBreakable)
{
	bBreakable = bIsBreakable;
	if (BreakableComponent)
	{
		BreakableComponent->bBreakable = bIsBreakable;
	}
}

void AGSBlockBase::ApplyCurrentConfiguration()
{
	if (Mesh)
	{
		Mesh->SetSimulatePhysics(bStartSimulatingPhysics);
		Mesh->SetEnableGravity(false);
		Mesh->SetUseCCD(bUseContinuousCollisionDetection);
		// 落地不弹:零回弹覆盖(Combine=Min,对面材质再弹也取最小)——
		// ±Z 切换落下的箱子要稳稳停住,不是弹球。
		// ⚠ 函数级静态指针不进 GC 引用图:PIE 重启间隙材质可能被回收,
		// 下局设悬空 override 会触发 BodyInstance 断言崩溃(实测)——必须 AddToRoot。
		if (bZeroBounceOnLand)
		{
			static UPhysicalMaterial* ZeroBounceMat = nullptr;
			if (!ZeroBounceMat || !ZeroBounceMat->IsValidLowLevel())
			{
				ZeroBounceMat = NewObject<UPhysicalMaterial>(GetTransientPackage(), TEXT("GSBlockZeroBounce"));
				ZeroBounceMat->Restitution = 0.0f;
				ZeroBounceMat->bOverrideRestitutionCombineMode = true;
				ZeroBounceMat->RestitutionCombineMode = EFrictionCombineMode::Min;
				ZeroBounceMat->AddToRoot();
			}
			Mesh->SetPhysMaterialOverride(ZeroBounceMat);
		}
		// 玩家推不动:质量方案(§20 同款)。自定义重力走 bAccelChange(质量无关),
		// 抬质量只影响被撞时的位移,不影响 ±Z 切换的升/降。
		const float EffectiveMassKg = bImmovableByPlayer ? FMath::Max(MassOverrideKg, ImmovableMassKg) : MassOverrideKg;
		if (bStartSimulatingPhysics && EffectiveMassKg > 0.0f)
		{
			Mesh->SetMassOverrideInKg(NAME_None, EffectiveMassKg, true);
		}
	}

	if (GravityBody)
	{
		GravityBody->SetTargetPrimitive(Mesh);
		GravityBody->bGravityEnabled = bAffectedByGravity;
		GravityBody->bUseContinuousCollisionDetection = bUseContinuousCollisionDetection;
		// 悬浮待命期:GravityScale 按 0 计(准星仍可锁定——bGravityEnabled 之外都不动),
		// 首次点击唤醒后恢复配置值(见 ToggleGravityZ)。
		const bool bPendingFirstClick = bStartStaticUntilClicked && !bActivatedByClick;
		GravityBody->GravityScale = bPendingFirstClick ? 0.0f : GravityScale;
		if (bPendingFirstClick)
		{
			FreezeMotion();
		}
		GravityBody->MaximumSpeedCm = MaximumSpeedCm;
		GravityBody->BaseImpactEnergyMultiplier = ImpactEnergyMultiplier;
		GravityBody->ImpactSourceTag = ImpactSourceTag;
		GravityBody->bCanBreakTargets = bCanBreakTargets;
		// 新机制:物体重力恒定(方向由 bGravityRises / GravityAxisLocal 定),不再跟随
		// 管理器提交的全局方向——玩家转向器/落地反转不再带动方块,方块重力只由
		// 准星瞄准+左键改变(掉下来 ↔ 升起来)。
		GravityBody->SetOwnGravityDirection(GetGravityAxisWorld(), true);
		GravityBody->RefreshReferences();
	}

	if (BreakableComponent)
	{
		BreakableComponent->SetTargetPrimitive(Mesh);
		BreakableComponent->bBreakable = bBreakable;
	}

	if (Resettable)
	{
		Resettable->CaptureInitialState();
	}
}
