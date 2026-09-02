#include "GSBlockBase.h"

#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

#include "GSBreakableComponent.h"
#include "GSGravityBodyComponent.h"
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

	GravityBody = CreateDefaultSubobject<UGSGravityBodyComponent>(TEXT("GravityBody"));
	SurfaceReceiver = CreateDefaultSubobject<UGSSurfaceReceiverComponent>(TEXT("SurfaceReceiver"));
	BreakableComponent = CreateDefaultSubobject<UGSBreakableComponent>(TEXT("Breakable"));
	Resettable = CreateDefaultSubobject<UGSResettableComponent>(TEXT("Resettable"));
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
		if (bStartSimulatingPhysics && MassOverrideKg > 0.0f)
		{
			Mesh->SetMassOverrideInKg(NAME_None, MassOverrideKg, true);
		}
	}

	if (GravityBody)
	{
		GravityBody->SetTargetPrimitive(Mesh);
		GravityBody->bGravityEnabled = bAffectedByGravity;
		GravityBody->bUseContinuousCollisionDetection = bUseContinuousCollisionDetection;
		GravityBody->GravityScale = GravityScale;
		GravityBody->MaximumSpeedCm = MaximumSpeedCm;
		GravityBody->BaseImpactEnergyMultiplier = ImpactEnergyMultiplier;
		GravityBody->ImpactSourceTag = ImpactSourceTag;
		GravityBody->bCanBreakTargets = bCanBreakTargets;
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
