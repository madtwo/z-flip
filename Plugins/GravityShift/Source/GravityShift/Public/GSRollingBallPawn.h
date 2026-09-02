// GravityShift v5 - physics rolling ball. No CharacterMovement, no capsule.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "GravityShiftTypes.h"
#include "GSRollingBallPawn.generated.h"

class AGSGravityManager;
class AGSWorldStateManager;
class UCameraComponent;
class UGSBallProfile;
class UGSGravityBodyComponent;
class UGSLandingProfile;
class UGSLandingResponseComponent;
class UGSResettableComponent;
class UGSSurfaceReceiverComponent;
class USceneComponent;
class USphereComponent;
class USpringArmComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Rolling Ball Pawn"))
class GRAVITYSHIFT_API AGSRollingBallPawn : public APawn
{
	GENERATED_BODY()

public:
	AGSRollingBallPawn();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USphereComponent> BallCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> BallMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USceneComponent> CameraPivot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<USpringArmComponent> CameraArm = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UCameraComponent> Camera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSGravityBodyComponent> GravityBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSSurfaceReceiverComponent> SurfaceReceiver = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSLandingResponseComponent> LandingResponse = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSResettableComponent> Resettable = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBallProfile> BallProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float StopTorqueAcceleration = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSGravityManager> GravityManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AGSWorldStateManager> WorldStateManager = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSLandingResponseComponent> LandingResponseRef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	bool bEnableNativePollingInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	bool bAutoPossessFirstPlayer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey ForwardKey = EKeys::W;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey BackwardKey = EKeys::S;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey LeftKey = EKeys::A;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey RightKey = EKeys::D;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey FlipGravityKey = EKeys::G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey InteractKey = EKeys::E;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey ResetKey = EKeys::R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bAllowAirControl = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bClampPlanarSpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float RollTorqueAcceleration = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float MaximumPlanarSpeedCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float AirControlAccelerationCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bCameraFlipsWithGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.01"))
	float CameraFlipDurationSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraFollowInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraYawDegreesPerMouseUnit = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraPitchDegreesPerMouseUnit = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaximumCameraPitchDegrees = 70.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraPitchDegrees = -12.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraArmLengthCm = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Interaction", meta = (ClampMin = "0.0"))
	float InteractionRadiusCm = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity")
	float ManualVelocityRetention = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity")
	float AutomaticVelocityRetention = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity", meta = (ClampMin = "0.0"))
	float AutomaticMaxCarrySpeedCm = 700.0f;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyBallProfile(UGSBallProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void RefreshSystemReferences();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetMoveInput(FVector2D NewMoveInput);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void AddCameraLookInput(float YawDeltaDegrees, float PitchDeltaDegrees);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestManualGravityFlip();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestGravityPolarity(EGSGravityPolarity NewPolarity, bool bForce);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FText GetCurrentInteractionText() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetToCheckpoint();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetBallLinearVelocity() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetBallLinearVelocity(FVector NewVelocity, bool bAddToCurrent);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetCameraUpVector() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FVector GetTargetCameraUpVector() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	EGSGravityPolarity GetCurrentGravityPolarity() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool DoesGravityFlipRotateBall() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	USphereComponent* GetBallCollisionComponent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	UCameraComponent* GetBallCameraComponent() const;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

protected:
	FVector2D MoveInput = FVector2D::ZeroVector;
	FVector CurrentCameraUp = FVector::UpVector;
	FVector TargetCameraUp = FVector::UpVector;
	FQuat CurrentCameraRotation = FQuat::Identity;
	bool bCameraRotationReady = false;
	bool bFlipKeyWasDown = false;
	bool bInteractKeyWasDown = false;
	bool bResetKeyWasDown = false;

	FVector GetActiveGravityDirection() const;
	FQuat BuildCameraRotation(const FVector& UpVector) const;
	void UpdateCamera(float DeltaSeconds);
	void ApplyMovement(float DeltaSeconds);
	void PollNativeInput();
	void HandleFlipPressed();
	AActor* FindBestInteractable() const;

	UFUNCTION()
	void HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason);
};
