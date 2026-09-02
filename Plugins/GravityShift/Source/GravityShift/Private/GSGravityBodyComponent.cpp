#include "GSGravityBodyComponent.h"

#include "GameFramework/Actor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GSBreakableComponent.h"
#include "GSGravityManager.h"
#include "GSSurfaceReceiverComponent.h"

UGSGravityBodyComponent::UGSGravityBodyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bWantsInitializeComponent = true;
}

void UGSGravityBodyComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshReferences();

	if (TargetPrimitive)
	{
		TargetPrimitive->OnComponentHit.AddDynamic(this, &UGSGravityBodyComponent::HandleTargetHit);
	}
}

void UGSGravityBodyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GravityManager)
	{
		GravityManager->OnGravityChanged.RemoveDynamic(this, &UGSGravityBodyComponent::HandleGravityChanged);
		GravityManager->UnregisterGravityBody(this);
	}

	if (TargetPrimitive)
	{
		TargetPrimitive->OnComponentHit.RemoveDynamic(this, &UGSGravityBodyComponent::HandleTargetHit);
	}

	Super::EndPlay(EndPlayReason);
}

void UGSGravityBodyComponent::RefreshReferences()
{
	AActor* OwnerActor = GetOwner();

	if (bAutoResolveTarget || !TargetPrimitive)
	{
		UPrimitiveComponent* Found = nullptr;
		if (OwnerActor)
		{
			Found = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
			if (!Found && OwnerActor->GetRootComponent())
			{
				Found = OwnerActor->FindComponentByClass<UStaticMeshComponent>();
			}
		}
		if (Found)
		{
			SetTargetPrimitive(Found);
		}
	}

	if (bAutoFindManager && !GravityManager)
	{
		GravityManager = AGSGravityManager::FindGravityManager(this);
	}

	// Register/bind here rather than BeginPlay: level-placed actors run BeginPlay
	// before the game mode auto-spawns the manager, so the first attempt comes up
	// empty and the body would never flip or wake. Both calls are idempotent.
	if (GravityManager)
	{
		GravityManager->RegisterGravityBody(this);
		GravityManager->OnGravityChanged.AddUniqueDynamic(this, &UGSGravityBodyComponent::HandleGravityChanged);
	}

	if (!SurfaceReceiver && OwnerActor)
	{
		SurfaceReceiver = OwnerActor->FindComponentByClass<UGSSurfaceReceiverComponent>();
	}
}

void UGSGravityBodyComponent::SetTargetPrimitive(UPrimitiveComponent* NewTarget)
{
	TargetPrimitive = NewTarget;
	if (!TargetPrimitive)
	{
		return;
	}

	TargetPrimitive->SetEnableGravity(false);
	if (bUseContinuousCollisionDetection)
	{
		TargetPrimitive->SetUseCCD(true);
	}
}

void UGSGravityBodyComponent::SetGravityEnabled(bool bEnabled)
{
	bGravityEnabled = bEnabled;
}

FVector UGSGravityBodyComponent::GetGravityDirection() const
{
	return GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
}

FVector UGSGravityBodyComponent::GetLinearVelocity() const
{
	return TargetPrimitive ? TargetPrimitive->GetPhysicsLinearVelocity() : FVector::ZeroVector;
}

void UGSGravityBodyComponent::SetLinearVelocity(FVector NewVelocity, bool bAddToCurrent)
{
	if (!TargetPrimitive)
	{
		return;
	}

	const FVector Final = bAddToCurrent ? TargetPrimitive->GetPhysicsLinearVelocity() + NewVelocity : NewVelocity;
	TargetPrimitive->SetPhysicsLinearVelocity(Final);
}

FVector UGSGravityBodyComponent::GetCachedPrePhysicsVelocity() const
{
	return CachedPrePhysicsVelocity;
}

FGSImpactReport UGSGravityBodyComponent::GetLastImpactReport() const
{
	return LastImpactReport;
}

bool UGSGravityBodyComponent::IsSimulatingTarget() const
{
	return TargetPrimitive && TargetPrimitive->IsSimulatingPhysics();
}

void UGSGravityBodyComponent::HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason)
{
	// A sleeping rigid body ignores AddForce, so a body resting on a surface
	// would stay pinned after a polarity flip until something else woke it.
	if (TargetPrimitive)
	{
		TargetPrimitive->WakeRigidBody();
	}
}

void UGSGravityBodyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsSimulatingTarget())
	{
		CachedPrePhysicsVelocity = FVector::ZeroVector;
		return;
	}

	if (!GravityManager)
	{
		// Late-binding retry: the manager may spawn after this actor's BeginPlay
		// (level-placed blocks vs game-mode-spawned manager).
		RefreshReferences();
	}

	FVector Velocity = TargetPrimitive->GetPhysicsLinearVelocity();
	CachedPrePhysicsVelocity = Velocity;

	if (!bGravityEnabled || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector Dir = GetGravityDirection();
	float AccelCm = GravityManager ? GravityManager->GravityAccelerationCm : 1600.0f;
	float Scale = GravityScale;
	float MaxSpeed = MaximumSpeedCm;
	float ImpactMult = BaseImpactEnergyMultiplier;
	float DragAxis = GravityAxisDragHz;
	float DragTangent = TangentDragHz;

	if (SurfaceReceiver)
	{
		const FGSSurfaceModifierSpec Mod = SurfaceReceiver->GetEffectiveSurfaceModifier();
		Scale *= Mod.GravityScaleMultiplier;
		MaxSpeed *= Mod.MaximumSpeedMultiplier;
		ImpactMult *= Mod.ImpactEnergyMultiplier;
		DragAxis += Mod.GravityAxisDragHz;
		DragTangent += Mod.TangentDragHz;
	}

	// Mass-independent acceleration keeps blocks and the ball consistent.
	TargetPrimitive->AddForce(Dir * AccelCm * Scale, NAME_None, true);

	if (DragAxis > 0.0f || DragTangent > 0.0f)
	{
		const FVector NormalPart = Dir * FVector::DotProduct(Velocity, Dir);
		const FVector TangentPart = Velocity - NormalPart;
		Velocity = NormalPart * FMath::Exp(-DragAxis * DeltaTime) + TangentPart * FMath::Exp(-DragTangent * DeltaTime);
	}

	if (MaxSpeed > 0.0f)
	{
		const float Speed = Velocity.Size();
		if (Speed > MaxSpeed)
		{
			Velocity = Velocity.GetSafeNormal() * MaxSpeed;
		}
	}

	TargetPrimitive->SetPhysicsLinearVelocity(Velocity);
}

FGSImpactReport UGSGravityBodyComponent::EvaluateImpact(AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector OtherVelocity, FVector HitNormal, FVector ImpactPoint)
{
	FGSImpactReport Report;
	if (!IsSimulatingTarget() || !OtherActor)
	{
		return Report;
	}

	UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (double* Last = LastImpactTimeByActor.Find(OtherActor))
	{
		if (Now - *Last < RepeatedImpactCooldownSeconds)
		{
			return Report;
		}
	}
	LastImpactTimeByActor.Add(OtherActor, Now);

	FVector SelfVelocity = CachedPrePhysicsVelocity;
	if (SelfVelocity.IsNearlyZero())
	{
		SelfVelocity = TargetPrimitive->GetPhysicsLinearVelocity();
	}

	const FVector Relative = SelfVelocity - OtherVelocity;
	const float RelativeNormalCm = FMath::Abs(FVector::DotProduct(Relative, HitNormal));
	const float RelativeNormalMps = GSGravity::CmToM(RelativeNormalCm);
	const float MassKg = FMath::Max(TargetPrimitive->GetMass(), 0.01f);
	const float EnergyJ = 0.5f * MassKg * RelativeNormalMps * RelativeNormalMps * FMath::Max(BaseImpactEnergyMultiplier, 0.0f);

	Report.bValid = true;
	Report.EnergyJ = EnergyJ;
	Report.RelativeNormalSpeedCm = RelativeNormalCm;
	Report.RelativeNormalSpeedMps = RelativeNormalMps;
	Report.SourceTag = ImpactSourceTag;
	Report.OtherActor = OtherActor;
	Report.ImpactPoint = ImpactPoint;
	Report.ImpactNormal = HitNormal;
	Report.TimeSeconds = Now;
	LastImpactReport = Report;

	if (bCanBreakTargets)
	{
		if (UGSBreakableComponent* OtherBreakable = OtherActor->FindComponentByClass<UGSBreakableComponent>())
		{
			OtherBreakable->ApplyImpactEnergy(EnergyJ, GetOwner());
		}
	}

	return Report;
}

void UGSGravityBodyComponent::HandleTargetHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bCanBreakTargets || !OtherActor)
	{
		return;
	}

	const FVector OtherVelocity = OtherComp && OtherComp->IsSimulatingPhysics() ? OtherComp->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	EvaluateImpact(OtherActor, OtherComp, OtherVelocity, Hit.ImpactNormal, Hit.ImpactPoint);
}
