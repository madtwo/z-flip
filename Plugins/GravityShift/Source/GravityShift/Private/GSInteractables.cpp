#include "GSInteractables.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

#include "GSGravityManager.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
#include "GSRollingBallPawn.h"
#include "GSSurfaceReceiverComponent.h"

// ---------------------------------------------------------------------------------
// Surface modifier volume
// ---------------------------------------------------------------------------------

AGSSurfaceModifierVolume::AGSSurfaceModifierVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(VolumeExtent);
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);
	SetRootComponent(Volume);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Volume);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeAsset.Object);
	}

	FallbackProfileA.Priority = 10;
	FallbackProfileB.Priority = 10;
}

void AGSSurfaceModifierVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetVolumeExtent(VolumeExtent);
	if (VisualMesh)
	{
		VisualMesh->SetHiddenInGame(!bShowVisualMesh);
		VisualMesh->SetVisibility(bShowVisualMesh);
	}
}

void AGSSurfaceModifierVolume::BeginPlay()
{
	Super::BeginPlay();

	Volume->OnComponentBeginOverlap.AddDynamic(this, &AGSSurfaceModifierVolume::HandleBeginOverlap);
	Volume->OnComponentEndOverlap.AddDynamic(this, &AGSSurfaceModifierVolume::HandleEndOverlap);
}

void AGSSurfaceModifierVolume::SetVolumeExtent(FVector NewExtent)
{
	VolumeExtent = NewExtent;
	if (Volume)
	{
		Volume->SetBoxExtent(NewExtent);
	}
	if (VisualMesh)
	{
		// /Engine/BasicShapes/Cube is 100cm on a side, extent is a half-size.
		VisualMesh->SetRelativeScale3D(NewExtent / 50.0f);
	}
}

FGSSurfaceModifierSpec AGSSurfaceModifierVolume::GetActiveSurfaceModifier() const
{
	if (ActiveProfileIndex == 0)
	{
		return ProfileA ? ProfileA->Spec : FallbackProfileA;
	}
	return ProfileB ? ProfileB->Spec : FallbackProfileB;
}

void AGSSurfaceModifierVolume::SetActiveProfileIndex(int32 NewIndex)
{
	ActiveProfileIndex = NewIndex;
	ReapplyToOverlappingActors();
}

void AGSSurfaceModifierVolume::ToggleProfile()
{
	ActiveProfileIndex = ActiveProfileIndex == 0 ? 1 : 0;
	ReapplyToOverlappingActors();
}

void AGSSurfaceModifierVolume::ReapplyToOverlappingActors()
{
	if (!Volume)
	{
		return;
	}

	TArray<AActor*> Overlapping;
	Volume->GetOverlappingActors(Overlapping);

	for (AActor* Actor : Overlapping)
	{
		if (!Actor)
		{
			continue;
		}

		UGSSurfaceReceiverComponent* Receiver = Actor->FindComponentByClass<UGSSurfaceReceiverComponent>();
		if (Receiver)
		{
			Receiver->ApplySurfaceModifier(this, GetActiveSurfaceModifier());
		}
	}
}

void AGSSurfaceModifierVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bEnabled || !OtherActor)
	{
		return;
	}

	if (UGSSurfaceReceiverComponent* Receiver = OtherActor->FindComponentByClass<UGSSurfaceReceiverComponent>())
	{
		Receiver->ApplySurfaceModifier(this, GetActiveSurfaceModifier());
	}
}

void AGSSurfaceModifierVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	if (UGSSurfaceReceiverComponent* Receiver = OtherActor->FindComponentByClass<UGSSurfaceReceiverComponent>())
	{
		Receiver->RemoveSurfaceModifier(this);
	}
}

// ---------------------------------------------------------------------------------
// Landing response volume
// ---------------------------------------------------------------------------------

AGSLandingResponseVolume::AGSLandingResponseVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	Volume = CreateDefaultSubobject<UBoxComponent>(TEXT("Volume"));
	Volume->SetBoxExtent(VolumeExtent);
	Volume->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Volume->SetGenerateOverlapEvents(true);
	SetRootComponent(Volume);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(Volume);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		VisualMesh->SetStaticMesh(CubeAsset.Object);
	}

	FallbackProfileA.Priority = 10;
	FallbackProfileB.Priority = 10;
}

void AGSLandingResponseVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SetVolumeExtent(VolumeExtent);
	if (VisualMesh)
	{
		VisualMesh->SetHiddenInGame(!bShowVisualMesh);
		VisualMesh->SetVisibility(bShowVisualMesh);
	}
}

void AGSLandingResponseVolume::BeginPlay()
{
	Super::BeginPlay();

	Volume->OnComponentBeginOverlap.AddDynamic(this, &AGSLandingResponseVolume::HandleBeginOverlap);
	Volume->OnComponentEndOverlap.AddDynamic(this, &AGSLandingResponseVolume::HandleEndOverlap);
}

void AGSLandingResponseVolume::SetVolumeExtent(FVector NewExtent)
{
	VolumeExtent = NewExtent;
	if (Volume)
	{
		Volume->SetBoxExtent(NewExtent);
	}
	if (VisualMesh)
	{
		VisualMesh->SetRelativeScale3D(NewExtent / 50.0f);
	}
}

FGSLandingModifierSpec AGSLandingResponseVolume::GetActiveLandingModifier() const
{
	if (ActiveProfileIndex == 0)
	{
		return ProfileA ? ProfileA->Spec : FallbackProfileA;
	}
	return ProfileB ? ProfileB->Spec : FallbackProfileB;
}

void AGSLandingResponseVolume::SetActiveProfileIndex(int32 NewIndex)
{
	ActiveProfileIndex = NewIndex;
	ReapplyToOverlappingActors();
}

void AGSLandingResponseVolume::ToggleProfile()
{
	ActiveProfileIndex = ActiveProfileIndex == 0 ? 1 : 0;
	ReapplyToOverlappingActors();
}

void AGSLandingResponseVolume::ReapplyToOverlappingActors()
{
	if (!Volume)
	{
		return;
	}

	TArray<AActor*> Overlapping;
	Volume->GetOverlappingActors(Overlapping);

	for (AActor* Actor : Overlapping)
	{
		if (!Actor)
		{
			continue;
		}

		UGSLandingResponseComponent* Landing = Actor->FindComponentByClass<UGSLandingResponseComponent>();
		if (Landing)
		{
			Landing->ApplyLandingModifier(this, GetActiveLandingModifier());
		}
	}
}

void AGSLandingResponseVolume::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bEnabled || !OtherActor)
	{
		return;
	}

	if (UGSLandingResponseComponent* Landing = OtherActor->FindComponentByClass<UGSLandingResponseComponent>())
	{
		Landing->ApplyLandingModifier(this, GetActiveLandingModifier());
	}
}

void AGSLandingResponseVolume::HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor)
	{
		return;
	}

	if (UGSLandingResponseComponent* Landing = OtherActor->FindComponentByClass<UGSLandingResponseComponent>())
	{
		Landing->RemoveLandingModifier(this);
	}
}

// ---------------------------------------------------------------------------------
// Gravity switch
// ---------------------------------------------------------------------------------

AGSGravitySwitch::AGSGravitySwitch()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SwitchMesh"));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CubeAsset.Object);
	}

	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	Trigger->SetupAttachment(Mesh);
	Trigger->SetBoxExtent(FVector(120.0, 120.0, 120.0));
	Trigger->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	Trigger->SetGenerateOverlapEvents(true);
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AGSGravitySwitch::HandleBeginOverlap);
}

void AGSGravitySwitch::BeginPlay()
{
	Super::BeginPlay();

	if (!GravityManager)
	{
		GravityManager = AGSGravityManager::FindGravityManager(this);
	}
}

EGSGravityRequestResult AGSGravitySwitch::TriggerSwitch(AActor* RequestInstigator)
{
	if (!bEnabled)
	{
		return EGSGravityRequestResult::INVALID_REQUEST;
	}

	if (!GravityManager)
	{
		GravityManager = AGSGravityManager::FindGravityManager(this);
	}

	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	if (Now - LastTriggerTime < TriggerCooldownSeconds)
	{
		return EGSGravityRequestResult::REJECTED_COOLDOWN;
	}

	if (RemainingUses == 0)
	{
		return EGSGravityRequestResult::INVALID_REQUEST;
	}

	EGSGravityRequestResult Result = EGSGravityRequestResult::INVALID_REQUEST;
	switch (SwitchMode)
	{
	case EGSGravitySwitchMode::FORCE_NEGATIVE_Z:
		Result = GravityManager->RequestGravityPolarity(EGSGravityPolarity::NEGATIVE_Z, RequestInstigator ? RequestInstigator : this, EGSGravityChangeReason::SWITCH, bForceGravityRequest);
		break;
	case EGSGravitySwitchMode::FORCE_POSITIVE_Z:
		Result = GravityManager->RequestGravityPolarity(EGSGravityPolarity::POSITIVE_Z, RequestInstigator ? RequestInstigator : this, EGSGravityChangeReason::SWITCH, bForceGravityRequest);
		break;
	case EGSGravitySwitchMode::TOGGLE:
	default:
		Result = GravityManager->RequestToggleGravity(RequestInstigator ? RequestInstigator : this, EGSGravityChangeReason::SWITCH, bForceGravityRequest);
		break;
	}

	if (Result == EGSGravityRequestResult::ACCEPTED)
	{
		LastTriggerTime = Now;
		if (RemainingUses > 0)
		{
			--RemainingUses;
		}
	}

	return Result;
}

void AGSGravitySwitch::ResetUses(int32 NewUses)
{
	RemainingUses = NewUses;
	LastTriggerTime = -1000.0;
}

bool AGSGravitySwitch::CanInteract_Implementation(APawn* InstigatorPawn)
{
	return bEnabled && InstigatorPawn != nullptr && RemainingUses != 0;
}

bool AGSGravitySwitch::Interact_Implementation(APawn* InstigatorPawn)
{
	return TriggerSwitch(InstigatorPawn) == EGSGravityRequestResult::ACCEPTED;
}

FText AGSGravitySwitch::GetInteractionText_Implementation(APawn* InstigatorPawn)
{
	return FText::FromString(TEXT("Gravity Switch (E)"));
}

void AGSGravitySwitch::HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!bTriggerOnOverlap || !OtherActor || !OtherActor->IsA(APawn::StaticClass()))
	{
		return;
	}

	TriggerSwitch(OtherActor);
}

// ---------------------------------------------------------------------------------
// Surface controller device
// ---------------------------------------------------------------------------------

AGSSurfaceControllerDevice::AGSSurfaceControllerDevice()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DeviceMesh"));
	Mesh->SetCollisionProfileName(TEXT("BlockAll"));
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CubeAsset.Object);
	}
}

void AGSSurfaceControllerDevice::TriggerTargets()
{
	if (!bEnabled || RemainingUses == 0)
	{
		return;
	}

	for (const TObjectPtr<AGSSurfaceModifierVolume>& Volume : TargetSurfaceVolumes)
	{
		if (Volume)
		{
			Volume->ToggleProfile();
		}
	}

	for (const TObjectPtr<AGSLandingResponseVolume>& Volume : TargetLandingVolumes)
	{
		if (Volume)
		{
			Volume->ToggleProfile();
		}
	}

	if (RemainingUses > 0)
	{
		--RemainingUses;
	}
}

void AGSSurfaceControllerDevice::ResetUses(int32 NewUses)
{
	RemainingUses = NewUses;
}

bool AGSSurfaceControllerDevice::CanInteract_Implementation(APawn* InstigatorPawn)
{
	return bEnabled && InstigatorPawn != nullptr && RemainingUses != 0;
}

bool AGSSurfaceControllerDevice::Interact_Implementation(APawn* InstigatorPawn)
{
	TriggerTargets();
	return true;
}

FText AGSSurfaceControllerDevice::GetInteractionText_Implementation(APawn* InstigatorPawn)
{
	return FText::FromString(TEXT("Surface Controller (E)"));
}

// ---------------------------------------------------------------------------------
// Pickup item
// ---------------------------------------------------------------------------------

AGSPickupItem::AGSPickupItem()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	// NoCollision by default: pickups are collected by proximity + F, not by being
	// physical obstacles. Give the mesh a Blocking profile in the BP if it should
	// double as a prop the ball can rest against.
	Mesh->SetCollisionProfileName(TEXT("NoCollision"));
	Mesh->SetGenerateOverlapEvents(false);
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		Mesh->SetStaticMesh(CubeAsset.Object);
		Mesh->SetRelativeScale3D(FVector(0.4f));
	}
}

void AGSPickupItem::BeginPlay()
{
	Super::BeginPlay();
	bInitialCollected = bIsCollected;
}

bool AGSPickupItem::CanInteract_Implementation(APawn* InstigatorPawn)
{
	return !bIsCollected && InstigatorPawn != nullptr;
}

bool AGSPickupItem::Collect(APawn* Collector)
{
	if (bIsCollected)
	{
		return false;
	}

	bIsCollected = true;
	Mesh->SetHiddenInGame(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Pause input with a center-screen message so the player can read it. The ball
	// simply stops receiving move input (physics coasts it to rest); it is not frozen.
	if (AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(Collector))
	{
		if (!PickupMessage.IsEmpty())
		{
			Ball->ShowMessageAndLock(PickupMessage);
		}
	}

	return true;
}

bool AGSPickupItem::Interact_Implementation(APawn* InstigatorPawn)
{
	return Collect(InstigatorPawn);
}

FText AGSPickupItem::GetInteractionText_Implementation(APawn* InstigatorPawn)
{
	if (bIsCollected)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(TEXT("拾取 (F)"));
}

void AGSPickupItem::RestoreInitialState()
{
	bIsCollected = bInitialCollected;
	Mesh->SetHiddenInGame(false);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

// ---------------------------------------------------------------------------------
// Key
// ---------------------------------------------------------------------------------

AGSKey::AGSKey()
{
	// Keys behave exactly like pickups but unlock matching doors. Give the pickup
	// logic a sensible default read-out so a bare key already tells the player why.
	PickupMessage = FText::FromString(TEXT("你找到了一把钥匙，匹配的门被打开了。"));
}

bool AGSKey::Interact_Implementation(APawn* InstigatorPawn)
{
	const bool bSuccess = Super::Interact_Implementation(InstigatorPawn);
	if (bSuccess && !KeyID.IsNone())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<AGSDoor> It(World); It; ++It)
			{
				if (AGSDoor* Door = *It)
				{
					Door->TryUnlock(KeyID);
				}
			}
		}
	}
	return bSuccess;
}

FText AGSKey::GetInteractionText_Implementation(APawn* InstigatorPawn)
{
	if (bIsCollected)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(TEXT("拾取钥匙 (F)"));
}

// ---------------------------------------------------------------------------------
// Door
// ---------------------------------------------------------------------------------

AGSDoor::AGSDoor()
{
	PrimaryActorTick.bCanEverTick = true;

	DoorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DoorRoot"));
	SetRootComponent(DoorRoot);

	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(DoorRoot);
	DoorMesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	DoorMesh->SetGenerateOverlapEvents(false);

	// Default: a door panel whose base sits at DoorRoot. Swap the mesh/scale in BP.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeAsset.Succeeded())
	{
		DoorMesh->SetStaticMesh(CubeAsset.Object);
		DoorMesh->SetRelativeScale3D(FVector(1.6f, 0.3f, 2.2f));
		DoorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 110.0f));
	}
}

void AGSDoor::BeginPlay()
{
	Super::BeginPlay();
	bInitialLocked = bIsLocked;

	// A designer can start a door already unlocked; seat the panel at the open pose.
	if (!bIsLocked && DoorMesh)
	{
		SlideAlpha = 1.0f;
		DoorMesh->SetRelativeLocation(SlideOffset);
	}
}

bool AGSDoor::TryUnlock(FName KeyID)
{
	if (!bIsLocked)
	{
		return true;
	}
	if (RequiredKeyID != KeyID)
	{
		return false;
	}
	bIsLocked = false;
	return true;
}

void AGSDoor::SetLocked(bool bNowLocked)
{
	bIsLocked = bNowLocked;
}

void AGSDoor::RestoreInitialState()
{
	// Closing back to locked is driven by Tick once bIsLocked returns true.
	bIsLocked = bInitialLocked;
}

bool AGSDoor::CanInteract_Implementation(APawn* InstigatorPawn)
{
	return InstigatorPawn != nullptr && bIsLocked;
}

bool AGSDoor::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!bIsLocked)
	{
		return false;
	}

	// Pressing F on a locked door just explains the situation; no lock consumed.
	if (AGSRollingBallPawn* Ball = Cast<AGSRollingBallPawn>(InstigatorPawn))
	{
		if (!LockedMessage.IsEmpty())
		{
			Ball->ShowMessageAndLock(LockedMessage);
		}
	}
	return true;
}

FText AGSDoor::GetInteractionText_Implementation(APawn* InstigatorPawn)
{
	if (!bIsLocked)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(TEXT("锁住的门"));
}

void AGSDoor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const float Target = bIsLocked ? 0.0f : 1.0f;
	if (FMath::IsNearlyEqual(SlideAlpha, Target, 0.001f))
	{
		SlideAlpha = Target;
		return;
	}

	const float Step = (SlideDuration > KINDA_SMALL_NUMBER) ? (DeltaSeconds / SlideDuration) : 1.0f;
	SlideAlpha = bIsLocked
		? FMath::Max(SlideAlpha - Step, 0.0f)
		: FMath::Min(SlideAlpha + Step, 1.0f);

	if (DoorMesh)
	{
		DoorMesh->SetRelativeLocation(FMath::Lerp(FVector::ZeroVector, SlideOffset, SlideAlpha));
	}
}
