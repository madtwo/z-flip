#include "GSIntegratedDemoRoom.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

#include "GSBlockBase.h"
#include "GSInteractables.h"

AGSIntegratedDemoRoom::AGSIntegratedDemoRoom()
{
	PrimaryActorTick.bCanEverTick = false;

	FloorPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorPanel"));
	SetRootComponent(FloorPanel);
	FloorPanel->SetCollisionProfileName(TEXT("BlockAll"));

	CeilingPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CeilingPanel"));
	CeilingPanel->SetupAttachment(FloorPanel);
	CeilingPanel->SetCollisionProfileName(TEXT("BlockAll"));

	WallPosX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallPosX"));
	WallPosX->SetupAttachment(FloorPanel);
	WallPosX->SetCollisionProfileName(TEXT("BlockAll"));

	WallNegX = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallNegX"));
	WallNegX->SetupAttachment(FloorPanel);
	WallNegX->SetCollisionProfileName(TEXT("BlockAll"));

	WallPosY = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallPosY"));
	WallPosY->SetupAttachment(FloorPanel);
	WallPosY->SetCollisionProfileName(TEXT("BlockAll"));

	WallNegY = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallNegY"));
	WallNegY->SetupAttachment(FloorPanel);
	WallNegY->SetCollisionProfileName(TEXT("BlockAll"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		FloorPanel->SetStaticMesh(CubeAsset.Object);
		CeilingPanel->SetStaticMesh(CubeAsset.Object);
		WallPosX->SetStaticMesh(CubeAsset.Object);
		WallNegX->SetStaticMesh(CubeAsset.Object);
		WallPosY->SetStaticMesh(CubeAsset.Object);
		WallNegY->SetStaticMesh(CubeAsset.Object);
	}

	RefreshRoomGeometry();
}

void AGSIntegratedDemoRoom::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshRoomGeometry();
}

void AGSIntegratedDemoRoom::RefreshRoomGeometry()
{
	// /Engine/BasicShapes/Cube is 100cm on a side.
	const float X = RoomExtent.X;
	const float Y = RoomExtent.Y;
	const float T = PanelThickness;
	const float Height = FMath::Max(CeilingLocalZ - FloorLocalZ, T * 2.0f);

	if (FloorPanel)
	{
		FloorPanel->SetRelativeLocation(FVector(0.0, 0.0, FloorLocalZ - T * 0.5f));
		FloorPanel->SetRelativeScale3D(FVector(X * 2.0f / 100.0f, Y * 2.0f / 100.0f, T / 100.0f));
	}
	if (CeilingPanel)
	{
		CeilingPanel->SetRelativeLocation(FVector(0.0, 0.0, CeilingLocalZ + T * 0.5f));
		CeilingPanel->SetRelativeScale3D(FVector(X * 2.0f / 100.0f, Y * 2.0f / 100.0f, T / 100.0f));
	}
	if (WallPosX)
	{
		WallPosX->SetRelativeLocation(FVector(X + T * 0.5f, 0.0, FloorLocalZ + Height * 0.5f));
		WallPosX->SetRelativeScale3D(FVector(T / 100.0f, Y * 2.0f / 100.0f, Height / 100.0f));
	}
	if (WallNegX)
	{
		WallNegX->SetRelativeLocation(FVector(-X - T * 0.5f, 0.0, FloorLocalZ + Height * 0.5f));
		WallNegX->SetRelativeScale3D(FVector(T / 100.0f, Y * 2.0f / 100.0f, Height / 100.0f));
	}
	if (WallPosY)
	{
		WallPosY->SetRelativeLocation(FVector(0.0, Y + T * 0.5f, FloorLocalZ + Height * 0.5f));
		WallPosY->SetRelativeScale3D(FVector(X * 2.0f / 100.0f, T / 100.0f, Height / 100.0f));
	}
	if (WallNegY)
	{
		WallNegY->SetRelativeLocation(FVector(0.0, -Y - T * 0.5f, FloorLocalZ + Height * 0.5f));
		WallNegY->SetRelativeScale3D(FVector(X * 2.0f / 100.0f, T / 100.0f, Height / 100.0f));
	}
}

void AGSIntegratedDemoRoom::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnDemoActors();
	}
}

void AGSIntegratedDemoRoom::SpawnDemoActors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Base = GetActorLocation();
	const float MidZ = FloorLocalZ + (CeilingLocalZ - FloorLocalZ) * 0.5f;

	AGSBlockBase* Block = World->SpawnActor<AGSBlockBase>(AGSBlockBase::StaticClass(), FTransform(FRotator::ZeroRotator, Base + FVector(400.0, 0.0, MidZ)));
	if (Block)
	{
		Block->SetActorLabel(TEXT("DemoRoom_GravityBlock"));
		Block->SetAffectedByGravity(true);
		Block->SetSimulatingPhysics(true);
		Block->ApplyCurrentConfiguration();
	}

	AGSGravitySwitch* Switch = World->SpawnActor<AGSGravitySwitch>(AGSGravitySwitch::StaticClass(), FTransform(FRotator::ZeroRotator, Base + FVector(-400.0, 0.0, FloorLocalZ + 80.0)));
	if (Switch)
	{
		Switch->SetActorLabel(TEXT("DemoRoom_GravitySwitch"));
	}

	bDemoSpawned = true;
}
