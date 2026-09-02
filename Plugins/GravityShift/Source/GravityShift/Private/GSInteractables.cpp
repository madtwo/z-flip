#include "GSInteractables.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

#include "GSGravityManager.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
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
