#include "GSBreakableComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "TimerManager.h"

#include "GSGravityBodyComponent.h"
#include "GSProfiles.h"

UGSBreakableComponent::UGSBreakableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGSBreakableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetPrimitive)
	{
		AActor* OwnerActor = GetOwner();
		UPrimitiveComponent* Found = OwnerActor ? Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()) : nullptr;
		if (!Found && OwnerActor)
		{
			Found = OwnerActor->FindComponentByClass<UStaticMeshComponent>();
		}
		SetTargetPrimitive(Found);
	}

	CaptureInitialState();
}

void UGSBreakableComponent::SetTargetPrimitive(UPrimitiveComponent* NewTarget)
{
	TargetPrimitive = NewTarget;
}

void UGSBreakableComponent::CaptureInitialState()
{
	InitialHealth = MaximumHealth;
	CurrentHealth = MaximumHealth;
	bHasCapturedState = true;
}

void UGSBreakableComponent::ApplyBreakProfile(UGSBreakProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	BreakProfile = NewProfile;
	bBreakable = NewProfile->bBreakable;
	bOneHitBreakAboveThreshold = NewProfile->bOneHitBreakAboveThreshold;
	MinimumImpactEnergyJ = NewProfile->MinimumImpactEnergyJ;
	MaximumHealth = NewProfile->MaximumHealth;
	DamageScalePerJ = NewProfile->DamageScalePerJ;
	RequiredSourceTag = NewProfile->RequiredSourceTag;
	bHideOwnerWhenBroken = NewProfile->bHideOwnerWhenBroken;
	bDisableCollisionWhenBroken = NewProfile->bDisableCollisionWhenBroken;
	bDisablePhysicsWhenBroken = NewProfile->bDisablePhysicsWhenBroken;
	BrokenMesh = NewProfile->BrokenMesh;
	FractureCollection = NewProfile->FractureCollection;
	CurrentHealth = MaximumHealth;
}

bool UGSBreakableComponent::SourceTagMatches(AActor* Instigator) const
{
	if (RequiredSourceTag == NAME_None)
	{
		return true;
	}

	UGSGravityBodyComponent* Body = Instigator ? Instigator->FindComponentByClass<UGSGravityBodyComponent>() : nullptr;
	if (Body && Body->ImpactSourceTag == RequiredSourceTag)
	{
		return true;
	}

	return Instigator && Instigator->ActorHasTag(RequiredSourceTag);
}

float UGSBreakableComponent::ApplyImpactEnergy(float EnergyJ, AActor* Instigator)
{
	if (!bBreakable || bBroken || EnergyJ <= 0.0f)
	{
		return 0.0f;
	}

	if (!SourceTagMatches(Instigator))
	{
		return 0.0f;
	}

	if (EnergyJ < MinimumImpactEnergyJ)
	{
		return 0.0f;
	}

	const float Damage = EnergyJ * DamageScalePerJ;
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - Damage);

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] impact %.2fJ on %s -> health %.2f"), EnergyJ, *GetNameSafe(GetOwner()), CurrentHealth);

	if (bOneHitBreakAboveThreshold || CurrentHealth <= 0.0f)
	{
		BreakNow(Instigator, EnergyJ);
	}

	return Damage;
}

bool UGSBreakableComponent::BreakNow(AActor* Instigator, float EnergyJ)
{
	if (!bBreakable || bBroken)
	{
		return false;
	}

	bBroken = true;
	CurrentHealth = 0.0f;

	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* Prim = TargetPrimitive;
	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Prim);

	// 动手之前先记原状。
	if (MeshComp)
	{
		MeshBeforeBreak = MeshComp->GetStaticMesh();
	}
	if (Prim)
	{
		CollisionBeforeBreak = Prim->GetCollisionEnabled();
		bSimulatePhysicsBeforeBreak = Prim->IsSimulatingPhysics();
	}

	if (UGeometryCollection* Collection = ResolveFractureCollection())
	{
		// 碎裂:原网格就地让位给几何体集合。这里只能藏**组件**,不能
		// SetActorHiddenInGame —— 那会把刚建出来的碎块一起藏掉。
		if (Prim)
		{
			Prim->SetSimulatePhysics(false);
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Prim->SetVisibility(false, true);
		}
		SpawnFracture(Collection);
	}
	else
	{
		if (bDisablePhysicsWhenBroken && Prim)
		{
			Prim->SetSimulatePhysics(false);
		}
		if (bDisableCollisionWhenBroken && Prim)
		{
			Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		if (bHideOwnerWhenBroken && OwnerActor)
		{
			OwnerActor->SetActorHiddenInGame(true);
		}
	}

	if (MeshComp && BrokenMesh)
	{
		MeshComp->SetStaticMesh(BrokenMesh);
	}

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] %s BROKEN by %s (%.2fJ)"), *GetNameSafe(OwnerActor), *GetNameSafe(Instigator), EnergyJ);
	return true;
}

UGeometryCollection* UGSBreakableComponent::ResolveFractureCollection()
{
	if (FractureCollection)
	{
		return FractureCollection;
	}

	// 约定优于配置:关卡里 41 个可破坏件跨 6 张图,逐个填资产就要动 6 个 .umap,
	// 队友一同步就冲突。所以按网格名去约定目录里找 —— 资产丢进去就生效,关卡不用动。
	UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(TargetPrimitive);
	UStaticMesh* Mesh = MeshComp ? MeshComp->GetStaticMesh() : nullptr;
	if (!Mesh)
	{
		return nullptr;
	}

	const FString Name = Mesh->GetName();
	const FString Path = FString::Printf(TEXT("%s/%s_GC.%s_GC"), *FractureFolder, *Name, *Name);

	// ponytail: 同步加载,首次破坏会卡一帧;要挪干净就在关卡开始时预载。
	if (UGeometryCollection* Loaded = LoadObject<UGeometryCollection>(nullptr, *Path))
	{
		FractureCollection = Loaded;   // 缓存,后面的块直接命中
	}
	return FractureCollection;
}

void UGSBreakableComponent::SpawnFracture(UGeometryCollection* InCollection)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !InCollection)
	{
		return;
	}

	UGeometryCollectionComponent* GC = NewObject<UGeometryCollectionComponent>(OwnerActor);
	GC->SetMobility(EComponentMobility::Movable);
	OwnerActor->AddInstanceComponent(GC);
	GC->RegisterComponent();

	// 先摆回原网格的位姿(含缩放),再上资产——SetRestCollection 内部会 RecreatePhysicsState。
	// 用 world 而不是 relative:GC 是 AddInstanceComponent 上去的、没挂父,它的 relative
	// 就是 world;原网格却是挂在 Actor 根下的(AGSBlockBase 的 Mesh 恰好是根,
	// 但换个非根的网格组件 relative 就会错位)。
	// 资产必须烘成"网格局部空间"(源 Actor 缩放 1、无旋转),这里才成立。
	if (TargetPrimitive)
	{
		GC->SetWorldTransform(TargetPrimitive->GetComponentTransform());
	}
	GC->SetRestCollection(InCollection);
	GC->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	// 这一句才真正建出物理代理(SetSimulatePhysics 里 PhysicsProxy 为空时会补建)。
	GC->SetSimulatePhysics(true);
	FractureComponent = GC;

	// 代理是这一帧末尾才排进求解器的,当帧 Crumble 会静默无效,推到下一帧。
	if (UWorld* World = GetWorld())
	{
		TWeakObjectPtr<UGeometryCollectionComponent> WeakGC(GC);
		World->GetTimerManager().SetTimerForNextTick([WeakGC]()
		{
			if (UGeometryCollectionComponent* Pinned = WeakGC.Get())
			{
				Pinned->CrumbleActiveClusters();
			}
		});
	}
}

bool UGSBreakableComponent::Repair()
{
	if (!bBroken)
	{
		return false;
	}

	bBroken = false;
	CurrentHealth = bHasCapturedState ? InitialHealth : MaximumHealth;

	AActor* OwnerActor = GetOwner();
	UPrimitiveComponent* Prim = TargetPrimitive;

	// 碎块消失:整个组件连它的物理代理一起销毁,不留残骸。
	if (FractureComponent)
	{
		FractureComponent->DestroyComponent();
		FractureComponent = nullptr;
	}

	if (OwnerActor)
	{
		OwnerActor->SetActorHiddenInGame(false);
	}
	if (Prim)
	{
		// 把 BreakNow 动过的东西退回破坏前那一刻的样子(网格/可见性/碰撞/物理)。
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Prim))
		{
			if (MeshBeforeBreak)
			{
				MeshComp->SetStaticMesh(MeshBeforeBreak);
			}
		}
		Prim->SetVisibility(true, true);
		Prim->SetCollisionEnabled(CollisionBeforeBreak);
		Prim->SetSimulatePhysics(bSimulatePhysicsBeforeBreak);
	}

	UE_LOG(LogTemp, Log, TEXT("[GravityShift] %s repaired"), *GetNameSafe(OwnerActor));
	return true;
}

bool UGSBreakableComponent::IsBroken() const
{
	return bBroken;
}
