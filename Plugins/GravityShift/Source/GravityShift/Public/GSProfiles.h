// GravityShift v5 - profile DataAssets (canonical seed lives in CSV, these are the runtime assets).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GravityShiftTypes.h"
#include "GSProfiles.generated.h"

class UStaticMesh;

UCLASS(BlueprintType, meta = (DisplayName = "GS Gravity Profile"))
class GRAVITYSHIFT_API UGSGravityProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = TEXT("GRAVITY_DEFAULT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityAccelerationCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float ManualCooldownSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutomaticCooldownSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	EGSGravityPolarity DefaultPolarity = EGSGravityPolarity::NEGATIVE_Z;
};

UCLASS(BlueprintType, meta = (DisplayName = "GS Ball Profile"))
class GRAVITYSHIFT_API UGSBallProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = TEXT("BALL_DEFAULT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Body", meta = (ClampMin = "1.0"))
	float RadiusCm = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Body", meta = (ClampMin = "0.01"))
	float MassKg = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Body")
	float LinearDamping = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Body")
	float AngularDamping = 0.15f;

	// Angular acceleration applied as torque (rad/s^2), mass independent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float RollTorqueAcceleration = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float MaximumPlanarSpeedCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bClampPlanarSpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bAllowAirControl = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float AirControlAccelerationCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.01"))
	float CameraFlipDurationSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraFollowInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraArmLengthCm = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraYawDegreesPerMouseUnit = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraPitchDegreesPerMouseUnit = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float MaximumCameraPitchDegrees = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bCameraFlipsWithGravity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Gravity")
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Gravity", meta = (ClampMin = "0.0"))
	float GravityAxisDragHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Gravity", meta = (ClampMin = "0.0"))
	float TangentDragHz = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity")
	float ManualVelocityRetention = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity")
	float AutomaticVelocityRetention = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Velocity", meta = (ClampMin = "0.0"))
	float AutomaticMaxCarrySpeedCm = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Landing")
	EGSAutoReverseMode AutoReverseMode = EGSAutoReverseMode::LANDING_IMPACT;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Landing", meta = (ClampMin = "0.0"))
	float AutoReverseFallSpeedCm = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Landing", meta = (ClampMin = "0.0"))
	float AutoReverseFallDistanceCm = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Landing", meta = (ClampMin = "0.0"))
	float LandingAutoReverseAtSpeedCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Landing", meta = (ClampMin = "0.0"))
	float BounceSpeedCm = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Landing", meta = (ClampMin = "0.0"))
	float NoResponseBelowImpactSpeedCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Interaction", meta = (ClampMin = "0.0"))
	float InteractionRadiusCm = 320.0f;
};

UCLASS(BlueprintType, meta = (DisplayName = "GS Surface Profile"))
class GRAVITYSHIFT_API UGSSurfaceProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSSurfaceModifierSpec Spec;
};

UCLASS(BlueprintType, meta = (DisplayName = "GS Landing Profile"))
class GRAVITYSHIFT_API UGSLandingProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FGSLandingModifierSpec Spec;
};

UCLASS(BlueprintType, meta = (DisplayName = "GS Break Profile"))
class GRAVITYSHIFT_API UGSBreakProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = TEXT("BREAK_DEFAULT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bBreakable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bOneHitBreakAboveThreshold = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MinimumImpactEnergyJ = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.01"))
	float MaximumHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float DamageScalePerJ = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName RequiredSourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bHideOwnerWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bDisableCollisionWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bDisablePhysicsWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UStaticMesh> BrokenMesh = nullptr;
};

UCLASS(BlueprintType, meta = (DisplayName = "GS Block Profile"))
class GRAVITYSHIFT_API UGSBlockProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = TEXT("BLOCK_DEFAULT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UStaticMesh> Mesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bStartSimulatingPhysics = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bAffectedByGravity = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bCanBreakTargets = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bBreakable = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bUseContinuousCollisionDetection = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MassOverrideKg = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MaximumSpeedCm = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float ImpactEnergyMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ImpactSourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBreakProfile> BreakProfile = nullptr;
};

UCLASS(BlueprintType, meta = (DisplayName = "GS Collectible Profile"))
class GRAVITYSHIFT_API UGSCollectibleProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = TEXT("COLLECTIBLE_DEFAULT");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float Value = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bPersistThroughReset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bRequiredForGoal = false;
};
