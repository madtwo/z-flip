#include "GSRollingBallPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "UObject/ConstructorHelpers.h"

#include "GSGravityBodyComponent.h"
#include "GSGravityManager.h"
#include "GSInteractable.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
#include "GSRailCameraComponent.h"
#include "GSResettableComponent.h"
#include "GSSurfaceReceiverComponent.h"
#include "GSWorldState.h"

AGSRollingBallPawn::AGSRollingBallPawn()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	BallCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BallCollision"));
	BallCollision->SetSphereRadius(50.0f, false);
	BallCollision->SetSimulatePhysics(true);
	BallCollision->SetEnableGravity(false);
	BallCollision->SetCollisionProfileName(TEXT("PhysicsActor"));
	BallCollision->SetUseCCD(true);
	SetRootComponent(BallCollision);

	BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
	BallMesh->SetupAttachment(BallCollision);
	BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BallMesh->SetEnableGravity(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereAsset.Succeeded())
	{
		BallMesh->SetStaticMesh(SphereAsset.Object);
	}

	// Default tuning DataAsset (created by generate_data_assets.py). The game mode
	// spawns the pawn natively, so without this the ball runs on header defaults
	// (landing reverse at 900 instead of the profile's 1400 — every shed-height
	// landing flipped gravity).
	static ConstructorHelpers::FObjectFinder<UGSBallProfile> BallProfileAsset(TEXT("/Game/GravityShift/Data/Profiles/DA_GS_Ball_Default"));
	if (BallProfileAsset.Succeeded())
	{
		BallProfile = BallProfileAsset.Object;
	}

	CameraPivot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraPivot"));
	CameraPivot->SetupAttachment(BallCollision);
	// UpdateCamera writes an absolute world rotation every tick; treat it as
	// absolute so the rolling ball's rotation never bleeds into the camera rig.
	CameraPivot->SetUsingAbsoluteRotation(true);

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(CameraPivot);
	CameraArm->bUsePawnControlRotation = false;
	// The gravity flip is a pitch rotation on CameraPivot; the arm must pass
	// pivot rotation through to the camera or the flip never reaches the view.
	CameraArm->bInheritPitch = true;
	CameraArm->bInheritYaw = true;
	CameraArm->bInheritRoll = true;
	CameraArm->TargetArmLength = CameraArmLengthCm;
	CameraArm->SetUsingAbsoluteRotation(false);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraArm);
	Camera->bUsePawnControlRotation = false;

	GravityBody = CreateDefaultSubobject<UGSGravityBodyComponent>(TEXT("GravityBody"));
	SurfaceReceiver = CreateDefaultSubobject<UGSSurfaceReceiverComponent>(TEXT("SurfaceReceiver"));
	LandingResponse = CreateDefaultSubobject<UGSLandingResponseComponent>(TEXT("LandingResponse"));
	Resettable = CreateDefaultSubobject<UGSResettableComponent>(TEXT("Resettable"));
	RailCamera = CreateDefaultSubobject<UGSRailCameraComponent>(TEXT("RailCamera"));

	LandingResponseRef = LandingResponse;
}

void AGSRollingBallPawn::BeginPlay()
{
	Super::BeginPlay();

	RefreshSystemReferences();

	if (BallProfile)
	{
		ApplyBallProfile(BallProfile);
	}

	CameraArm->TargetArmLength = CameraArmLengthCm;

	TargetCameraUp = bCameraFlipsWithGravity ? -GetActiveGravityDirection() : FVector::UpVector;
	CurrentCameraUp = TargetCameraUp;
	CurrentCameraRotation = BuildCameraRotation(TargetCameraUp);
	bCameraRotationReady = true;

	if (bAutoPossessFirstPlayer)
	{
		AutoPossessPlayer = EAutoReceiveInput::Player0;
	}

	if (Resettable)
	{
		Resettable->CaptureInitialState();
	}
}

void AGSRollingBallPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	// Input is polled in Tick: BindAction on a level-placed instance loses its InputComponent
	// in this project (documented pitfall). Polling IsInputKeyDown is immune to input-stack timing.
}

void AGSRollingBallPawn::RefreshSystemReferences()
{
	if (!GravityManager)
	{
		GravityManager = AGSGravityManager::FindGravityManager(this);
	}

	if (!WorldStateManager)
	{
		WorldStateManager = AGSWorldStateManager::FindWorldStateManager(this);
		if (WorldStateManager)
		{
			WorldStateManager->RegisterPlayer(this);
		}
	}

	if (GravityManager)
	{
		GravityManager->OnGravityChanged.RemoveDynamic(this, &AGSRollingBallPawn::HandleGravityChanged);
		GravityManager->OnGravityChanged.AddDynamic(this, &AGSRollingBallPawn::HandleGravityChanged);
	}

	if (GravityBody)
	{
		GravityBody->RefreshReferences();
	}

	if (LandingResponse)
	{
		LandingResponse->RefreshReferences();
	}
}

void AGSRollingBallPawn::ApplyBallProfile(UGSBallProfile* NewProfile)
{
	if (!NewProfile)
	{
		return;
	}

	BallProfile = NewProfile;

	if (BallCollision)
	{
		BallCollision->SetSphereRadius(NewProfile->RadiusCm, false);
		BallCollision->SetMassOverrideInKg(NAME_None, NewProfile->MassKg, true);
		BallCollision->SetLinearDamping(NewProfile->LinearDamping);
		BallCollision->SetAngularDamping(NewProfile->AngularDamping);
	}

	if (BallMesh)
	{
		// /Engine/BasicShapes/Sphere has a 50cm radius.
		const float Scale = NewProfile->RadiusCm / 50.0f;
		BallMesh->SetWorldScale3D(FVector(Scale));
	}

	RollTorqueAcceleration = NewProfile->RollTorqueAcceleration;
	MaximumPlanarSpeedCm = NewProfile->MaximumPlanarSpeedCm;
	bClampPlanarSpeed = NewProfile->bClampPlanarSpeed;
	bAllowAirControl = NewProfile->bAllowAirControl;
	AirControlAccelerationCm = NewProfile->AirControlAccelerationCm;

	CameraFlipDurationSeconds = NewProfile->CameraFlipDurationSeconds;
	CameraFollowInterpSpeed = NewProfile->CameraFollowInterpSpeed;
	CameraArmLengthCm = NewProfile->CameraArmLengthCm;
	CameraYawDegreesPerMouseUnit = NewProfile->CameraYawDegreesPerMouseUnit;
	CameraPitchDegreesPerMouseUnit = NewProfile->CameraPitchDegreesPerMouseUnit;
	MaximumCameraPitchDegrees = NewProfile->MaximumCameraPitchDegrees;
	bCameraFlipsWithGravity = NewProfile->bCameraFlipsWithGravity;

	ManualVelocityRetention = NewProfile->ManualVelocityRetention;
	AutomaticVelocityRetention = NewProfile->AutomaticVelocityRetention;
	AutomaticMaxCarrySpeedCm = NewProfile->AutomaticMaxCarrySpeedCm;
	InteractionRadiusCm = NewProfile->InteractionRadiusCm;

	if (GravityBody)
	{
		GravityBody->GravityScale = NewProfile->GravityScale;
		GravityBody->GravityAxisDragHz = NewProfile->GravityAxisDragHz;
		GravityBody->TangentDragHz = NewProfile->TangentDragHz;
		GravityBody->MaximumSpeedCm = NewProfile->MaximumPlanarSpeedCm * 4.0f;
	}

	if (LandingResponse)
	{
		LandingResponse->AutoReverseMode = NewProfile->AutoReverseMode;
		LandingResponse->AutoReverseFallSpeedCm = NewProfile->AutoReverseFallSpeedCm;
		LandingResponse->AutoReverseFallDistanceCm = NewProfile->AutoReverseFallDistanceCm;
		LandingResponse->LandingAutoReverseAtSpeedCm = NewProfile->LandingAutoReverseAtSpeedCm;
		LandingResponse->BounceSpeedCm = NewProfile->BounceSpeedCm;
		LandingResponse->NoResponseBelowImpactSpeedCm = NewProfile->NoResponseBelowImpactSpeedCm;
		LandingResponse->AutomaticVelocityRetention = NewProfile->AutomaticVelocityRetention;
		LandingResponse->AutomaticMaxCarrySpeedCm = NewProfile->AutomaticMaxCarrySpeedCm;
	}
}

FVector AGSRollingBallPawn::GetActiveGravityDirection() const
{
	return GravityManager ? GravityManager->GetGravityDirection() : FVector(0.0, 0.0, -1.0);
}

FQuat AGSRollingBallPawn::BuildCameraRotation(const FVector& UpVector) const
{
	FVector Up = UpVector.GetSafeNormal();
	if (Up.IsNearlyZero())
	{
		Up = FVector::UpVector;
	}

	const FQuat AlignUp = FQuat::FindBetweenNormals(FVector::UpVector, Up);
	const FQuat YawQuat = FQuat(Up, FMath::DegreesToRadians(CameraYawDegrees));
	const FQuat Base = YawQuat * AlignUp;

	const FVector Right = Base.RotateVector(FVector::RightVector);
	const FQuat PitchQuat = FQuat(Right, FMath::DegreesToRadians(-CameraPitchDegrees));
	return PitchQuat * Base;
}

void AGSRollingBallPawn::UpdateCamera(float DeltaSeconds)
{
	if (!CameraPivot)
	{
		return;
	}

	// Rail camera: the position rides the level's camera rail and the view is
	// rebuilt around world up with a small clamped gimbal. The chase rig below
	// stays as the fallback for levels without a rail.
	FVector RailPosition = FVector::ZeroVector;
	FQuat RailRotation = FQuat::Identity;
	if (RailCamera && RailCamera->ComputeCameraPose(RailPosition, RailRotation, DeltaSeconds))
	{
		if (!bRailCamActive)
		{
			bRailCamActive = true;
			CameraArm->bDoCollisionTest = false;
			CameraArm->TargetArmLength = 0.0f;
		}
		CameraPivot->SetWorldLocationAndRotation(RailPosition, RailRotation);
		CurrentCameraUp = RailRotation.RotateVector(FVector::UpVector);
		return;
	}

	if (bRailCamActive)
	{
		bRailCamActive = false;
		CameraArm->bDoCollisionTest = true;
		CameraArm->TargetArmLength = CameraArmLengthCm;
	}

	TargetCameraUp = bCameraFlipsWithGravity ? -GetActiveGravityDirection() : FVector::UpVector;
	const FQuat Target = BuildCameraRotation(TargetCameraUp);

	if (!bCameraRotationReady)
	{
		CurrentCameraRotation = Target;
		bCameraRotationReady = true;
	}
	else
	{
		// Quaternion slerp: safe across a 180 degree flip (never Euler interpolation).
		const float Alpha = 1.0f - FMath::Exp(-DeltaSeconds / FMath::Max(CameraFlipDurationSeconds, 0.001f));
		CurrentCameraRotation = FQuat::Slerp(CurrentCameraRotation, Target, Alpha);
	}

	const FVector Location = BallCollision ? BallCollision->GetComponentLocation() : GetActorLocation();
	CameraPivot->SetWorldLocationAndRotation(Location, CurrentCameraRotation);
	CurrentCameraUp = CurrentCameraRotation.RotateVector(FVector::UpVector);
}

void AGSRollingBallPawn::ApplyMovement(float DeltaSeconds)
{
	if (!BallCollision || !BallCollision->IsSimulatingPhysics())
	{
		return;
	}

	if (MoveInput.IsNearlyZero())
	{
		// No input: counter-torque brake so the ball stops quickly instead of
		// coasting on rolling friction (supported only; air keeps momentum).
		if (LandingResponse && LandingResponse->IsSupported())
		{
			const FVector BrakeUp = -GetActiveGravityDirection();
			const FVector Velocity = BallCollision->GetPhysicsLinearVelocity();
			const float NormalSpeed = FVector::DotProduct(Velocity, BrakeUp);
			FVector Planar = Velocity - BrakeUp * NormalSpeed;
			const float PlanarSpeed = Planar.Size();
			if (PlanarSpeed > 10.0f)
			{
				const FVector TorqueAxis = FVector::CrossProduct(BrakeUp, -Planar / PlanarSpeed);
				BallCollision->AddTorqueInRadians(TorqueAxis * StopTorqueAcceleration, NAME_None, true);
			}
		}
		return;
	}

	const FVector Up = -GetActiveGravityDirection();
	const FVector GravityDir = -Up;

	// Camera-relative movement basis: project the camera's OWN forward/right onto
	// the support plane. Deriving Right as Up x Forward flips it whenever Up flips,
	// while the rail camera keeps its world-up roll — A/D ended up mirrored on the
	// ceiling. The camera's right vector matches the screen on every surface.
	FVector Forward = CameraPivot ? CameraPivot->GetForwardVector() : GetActorForwardVector();
	FVector Right = CameraPivot ? CameraPivot->GetRightVector() : FVector::CrossProduct(Up, Forward);

	if (CameraPivot && FMath::Abs(Up.Z) < 0.5f)
	{
		// Wall: the camera's right is perpendicular to the wall, so the generic
		// projection collapses. Control spec: W/S roll horizontally along the
		// wall; A/D climb/descend — A climbs on the screen-left wall, D climbs on
		// the screen-right wall (gravity toward the camera's right = right wall).
		FVector Horizontal = FVector(Forward.X, Forward.Y, 0.0f);
		if (Horizontal.Normalize())
		{
			Forward = Horizontal;
			const float SideSign = FVector::DotProduct(GravityDir, CameraPivot->GetRightVector()) >= 0.0f ? 1.0f : -1.0f;
			Right = FVector(0.0, 0.0, SideSign);
		}
		// Degenerate (camera faces straight into the wall plane): keep the
		// generic projected basis computed above.
	}

	Forward = Forward - Up * FVector::DotProduct(Forward, Up);
	if (!Forward.Normalize())
	{
		Forward = GetActorForwardVector();
		Right = FVector::CrossProduct(Up, Forward);
	}
	Right = Right - Up * FVector::DotProduct(Right, Up);
	if (!Right.Normalize())
	{
		Right = FVector::CrossProduct(Up, Forward);
	}
	FVector Desired = Forward * MoveInput.Y + Right * MoveInput.X;
	if (!Desired.Normalize())
	{
		return;
	}

	const bool bSupported = LandingResponse ? LandingResponse->IsSupported() : false;

	if (bSupported)
	{
		// Rolling kinematics: torque axis T drives v_center = -wr*(Up x T), so the
		// axis that rolls toward Desired is (Up x Desired) — the old (Desired x Up)
		// rolled the ball backwards/mirrored.
		const FVector TorqueAxis = FVector::CrossProduct(Up, Desired);
		BallCollision->AddTorqueInRadians(TorqueAxis * RollTorqueAcceleration, NAME_None, true);
	}
	else if (bAllowAirControl)
	{
		BallCollision->AddForce(Desired * AirControlAccelerationCm, NAME_None, true);
	}

	if (bClampPlanarSpeed && MaximumPlanarSpeedCm > 0.0f)
	{
		FVector Velocity = BallCollision->GetPhysicsLinearVelocity();
		const float NormalSpeed = FVector::DotProduct(Velocity, Up);
		FVector Planar = Velocity - Up * NormalSpeed;
		const float PlanarSpeed = Planar.Size();
		if (PlanarSpeed > MaximumPlanarSpeedCm)
		{
			Planar = Planar.GetSafeNormal() * MaximumPlanarSpeedCm;
			BallCollision->SetPhysicsLinearVelocity(Planar + Up * NormalSpeed);
		}
	}
}

void AGSRollingBallPawn::PollNativeInput()
{
	if (!bEnableNativePollingInput)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	PC->GetInputMouseDelta(MouseX, MouseY);
	// Rail mode owns the camera; accumulated mouse offsets only apply to the
	// chase rig, otherwise they would suddenly apply on the next rail handoff.
	if (!RailCamera || !RailCamera->IsDriving())
	{
		if (!FMath::IsNearlyZero(MouseX) || !FMath::IsNearlyZero(MouseY))
		{
			AddCameraLookInput(MouseX * CameraYawDegreesPerMouseUnit, -MouseY * CameraPitchDegreesPerMouseUnit);
		}
	}

	const float AxisX = (PC->IsInputKeyDown(RightKey) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(LeftKey) ? 1.0f : 0.0f);
	const float AxisY = (PC->IsInputKeyDown(ForwardKey) ? 1.0f : 0.0f) - (PC->IsInputKeyDown(BackwardKey) ? 1.0f : 0.0f);
	SetMoveInput(FVector2D(AxisX, AxisY));

	const bool bFlipDown = PC->IsInputKeyDown(FlipGravityKey);
	if (bFlipDown && !bFlipKeyWasDown)
	{
		HandleFlipPressed();
	}
	bFlipKeyWasDown = bFlipDown;

	const bool bInteractDown = PC->IsInputKeyDown(InteractKey);
	if (bInteractDown && !bInteractKeyWasDown)
	{
		TryInteract();
	}
	bInteractKeyWasDown = bInteractDown;

	const bool bResetDown = PC->IsInputKeyDown(ResetKey);
	if (bResetDown && !bResetKeyWasDown)
	{
		ResetToCheckpoint();
	}
	bResetKeyWasDown = bResetDown;

	// Player camera-distance keys: each press steps the rail camera's trail.
	const bool bTrailCloserDown = PC->IsInputKeyDown(TrailCloserKey);
	if (bTrailCloserDown && !bTrailCloserKeyWasDown)
	{
		if (RailCamera)
		{
			RailCamera->AdjustTrailDistance(-1.0f);
		}
	}
	bTrailCloserKeyWasDown = bTrailCloserDown;

	const bool bTrailFartherDown = PC->IsInputKeyDown(TrailFartherKey);
	if (bTrailFartherDown && !bTrailFartherKeyWasDown)
	{
		if (RailCamera)
		{
			RailCamera->AdjustTrailDistance(1.0f);
		}
	}
	bTrailFartherKeyWasDown = bTrailFartherDown;

	// Set-axis keys 1/2/3 snap gravity to the positive direction of that axis.
	const bool bAxisSetXDown = PC->IsInputKeyDown(AxisSetXKey);
	if (bAxisSetXDown && !bAxisSetXWasDown)
	{
		HandleSetGravityAxis(EGSGravityAxis::X);
	}
	bAxisSetXWasDown = bAxisSetXDown;

	const bool bAxisSetYDown = PC->IsInputKeyDown(AxisSetYKey);
	if (bAxisSetYDown && !bAxisSetYWasDown)
	{
		HandleSetGravityAxis(EGSGravityAxis::Y);
	}
	bAxisSetYWasDown = bAxisSetYDown;

	const bool bAxisSetZDown = PC->IsInputKeyDown(AxisSetZKey);
	if (bAxisSetZDown && !bAxisSetZWasDown)
	{
		HandleSetGravityAxis(EGSGravityAxis::Z);
	}
	bAxisSetZWasDown = bAxisSetZDown;
}

void AGSRollingBallPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bInputLocked)
	{
		// A center-screen message is up: suppress all gameplay input (roll/flip/
		// interact/reset/axis). The ball is not force-frozen - it simply gets no
		// move input, so it coasts/brakes to rest naturally. Only the dismiss key
		// is watched while locked.
		const APlayerController* PC = Cast<APlayerController>(GetController());
		const bool bDismiss = PC && PC->IsInputKeyDown(DismissMessageKey);
		if (bDismiss && !bDismissKeyWasDown)
		{
			DismissPendingMessage();
		}
		bDismissKeyWasDown = bDismiss;

		if (!bInputLocked)
		{
			// Dismissed this frame - fall through to a normal pass so a held
			// movement key resumes immediately.
		}
		else
		{
			ApplyMovement(DeltaSeconds);
			UpdateCamera(DeltaSeconds);
			return;
		}
	}
	else
	{
		bDismissKeyWasDown = false;
	}

	PollNativeInput();
	ApplyMovement(DeltaSeconds);
	UpdateCamera(DeltaSeconds);
}

void AGSRollingBallPawn::ShowMessageAndLock(const FText& Message)
{
	PendingMessage = Message;
	bInputLocked = true;
	SetMoveInput(FVector2D::ZeroVector);
}

void AGSRollingBallPawn::DismissPendingMessage()
{
	if (bInputLocked)
	{
		bInputLocked = false;
		PendingMessage = FText::GetEmpty();
	}
}

bool AGSRollingBallPawn::IsMessageLocked() const
{
	return bInputLocked;
}

FText AGSRollingBallPawn::GetPendingMessage() const
{
	return PendingMessage;
}

FKey AGSRollingBallPawn::GetMessageDismissKey() const
{
	return DismissMessageKey;
}

void AGSRollingBallPawn::SetMoveInput(FVector2D NewMoveInput)
{
	MoveInput = NewMoveInput;
}

void AGSRollingBallPawn::AddCameraLookInput(float YawDeltaDegrees, float PitchDeltaDegrees)
{
	CameraYawDegrees += YawDeltaDegrees;
	CameraPitchDegrees = FMath::Clamp(CameraPitchDegrees + PitchDeltaDegrees, -MaximumCameraPitchDegrees, MaximumCameraPitchDegrees);
}

void AGSRollingBallPawn::HandleFlipPressed()
{
	RequestManualGravityFlip();
}

EGSGravityRequestResult AGSRollingBallPawn::RequestManualGravityFlip()
{
	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	const EGSGravityRequestResult Result = GravityManager->RequestToggleGravity(this, EGSGravityChangeReason::MANUAL, false);
	if (Result == EGSGravityRequestResult::ACCEPTED && BallCollision)
	{
		const FVector Velocity = BallCollision->GetPhysicsLinearVelocity();
		BallCollision->SetPhysicsLinearVelocity(Velocity * ManualVelocityRetention);
	}

	return Result;
}

EGSGravityRequestResult AGSRollingBallPawn::RequestGravityPolarity(EGSGravityPolarity NewPolarity, bool bForce)
{
	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	return GravityManager->RequestGravityPolarity(NewPolarity, this, EGSGravityChangeReason::SCRIPTED, bForce);
}

EGSGravityRequestResult AGSRollingBallPawn::RequestGravityDirection(EGSGravityDirection NewDirection, bool bForce)
{
	if (!GravityManager)
	{
		return EGSGravityRequestResult::NO_MANAGER;
	}

	return GravityManager->RequestGravityDirection(NewDirection, this, EGSGravityChangeReason::SCRIPTED, bForce);
}

EGSGravityDirection AGSRollingBallPawn::GetCurrentGravityDirection() const
{
	return GravityManager ? GravityManager->GetCurrentDirection() : EGSGravityDirection::NEGATIVE_Z;
}

void AGSRollingBallPawn::HandleSetGravityAxis(EGSGravityAxis Axis)
{
	if (!GravityManager)
	{
		return;
	}

	// 1/2/3 snap gravity to the positive direction of the pressed axis. Pressing
	// the already-active axis yields NO_CHANGE, which is harmless.
	const EGSGravityRequestResult Result = GravityManager->SetGravityAxis(Axis, this, false);
	if (Result == EGSGravityRequestResult::REJECTED_DISABLED)
	{
		ShowAxisDisabledHint(Axis);
	}
}

void AGSRollingBallPawn::ShowAxisDisabledHint(EGSGravityAxis Axis)
{
	AxisHintText = FString::Printf(TEXT("%s轴不可用"), *GSGravity::GetAxisDisplayName(Axis));
	AxisHintExpireTime = GetWorld() ? GetWorld()->GetTimeSeconds() + AxisHintLifetimeSeconds : -1.0f;
}

bool AGSRollingBallPawn::IsAxisHintActive() const
{
	const UWorld* World = GetWorld();
	return World && AxisHintExpireTime >= 0.0f && World->GetTimeSeconds() < AxisHintExpireTime;
}

FString AGSRollingBallPawn::GetAxisHintText() const
{
	return AxisHintText;
}

AActor* AGSRollingBallPawn::FindBestInteractable() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	AActor* Best = nullptr;
	float BestDistanceSq = InteractionRadiusCm * InteractionRadiusCm;

	APawn* SelfPawn = const_cast<AGSRollingBallPawn*>(this);
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == this || !Candidate->GetClass()->ImplementsInterface(UGSInteractable::StaticClass()))
		{
			continue;
		}

		// Skip candidates that currently refuse interaction (collected-and-hidden
		// pickups, exhausted switches) so they don't shadow a valid target nearby.
		if (!IGSInteractable::Execute_CanInteract(Candidate, SelfPawn))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
		if (DistanceSq <= BestDistanceSq)
		{
			BestDistanceSq = DistanceSq;
			Best = Candidate;
		}
	}

	return Best;
}

bool AGSRollingBallPawn::TryInteract()
{
	AActor* Target = FindBestInteractable();
	if (!Target)
	{
		return false;
	}

	if (IGSInteractable::Execute_CanInteract(Target, this))
	{
		return IGSInteractable::Execute_Interact(Target, this);
	}

	return false;
}

FText AGSRollingBallPawn::GetCurrentInteractionText() const
{
	AActor* Target = FindBestInteractable();
	if (Target && IGSInteractable::Execute_CanInteract(Target, const_cast<AGSRollingBallPawn*>(this)))
	{
		return IGSInteractable::Execute_GetInteractionText(Target, const_cast<AGSRollingBallPawn*>(this));
	}

	return FText::GetEmpty();
}

void AGSRollingBallPawn::ResetToCheckpoint()
{
	if (GravityManager)
	{
		GravityManager->ResetGravity(true);
	}

	if (Resettable)
	{
		if (WorldStateManager && WorldStateManager->HasActiveCheckpoint)
		{
			Resettable->TeleportAndReset(WorldStateManager->ActiveCheckpointTransform);
		}
		else
		{
			Resettable->RestoreInitialState();
		}
	}

	if (LandingResponse)
	{
		LandingResponse->ResetFlightState();
	}
}

FVector AGSRollingBallPawn::GetBallLinearVelocity() const
{
	return BallCollision ? BallCollision->GetPhysicsLinearVelocity() : FVector::ZeroVector;
}

void AGSRollingBallPawn::SetBallLinearVelocity(FVector NewVelocity, bool bAddToCurrent)
{
	if (!BallCollision)
	{
		return;
	}

	const FVector Final = bAddToCurrent ? BallCollision->GetPhysicsLinearVelocity() + NewVelocity : NewVelocity;
	BallCollision->SetPhysicsLinearVelocity(Final);
}

FVector AGSRollingBallPawn::GetCameraUpVector() const { return CurrentCameraUp; }
FVector AGSRollingBallPawn::GetTargetCameraUpVector() const { return TargetCameraUp; }

EGSGravityPolarity AGSRollingBallPawn::GetCurrentGravityPolarity() const
{
	return GravityManager ? GravityManager->CurrentPolarity : EGSGravityPolarity::NEGATIVE_Z;
}

bool AGSRollingBallPawn::DoesGravityFlipRotateBall() const { return false; }

USphereComponent* AGSRollingBallPawn::GetBallCollisionComponent() const { return BallCollision; }
UCameraComponent* AGSRollingBallPawn::GetBallCameraComponent() const { return Camera; }

void AGSRollingBallPawn::HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason)
{
	// Camera target only. The actor, collision sphere and mesh are never reoriented here,
	// so natural rolling angular state survives the flip.
	TargetCameraUp = bCameraFlipsWithGravity ? -GravityDirection : FVector::UpVector;
}
