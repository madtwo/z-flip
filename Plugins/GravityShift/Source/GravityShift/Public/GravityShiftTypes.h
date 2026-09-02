// GravityShift v5 - shared enums and structs.
// Contract: gravity has exactly two states, NEGATIVE_Z and POSITIVE_Z.

#pragma once

#include "CoreMinimal.h"
#include "GravityShiftTypes.generated.h"

UENUM(BlueprintType)
enum class EGSGravityPolarity : uint8
{
	NEGATIVE_Z UMETA(DisplayName = "-Z (floor down)"),
	POSITIVE_Z UMETA(DisplayName = "+Z (ceiling down)")
};

UENUM(BlueprintType)
enum class EGSGravityChangeReason : uint8
{
	MANUAL UMETA(DisplayName = "Manual"),
	FALL_THRESHOLD UMETA(DisplayName = "Fall threshold"),
	LANDING_RESPONSE UMETA(DisplayName = "Landing response"),
	SWITCH UMETA(DisplayName = "Switch"),
	SCRIPTED UMETA(DisplayName = "Scripted"),
	RESET UMETA(DisplayName = "Reset")
};

UENUM(BlueprintType)
enum class EGSGravityRequestResult : uint8
{
	ACCEPTED UMETA(DisplayName = "Accepted"),
	NO_CHANGE UMETA(DisplayName = "No change"),
	REJECTED_COOLDOWN UMETA(DisplayName = "Rejected (cooldown)"),
	REJECTED_LOCKED UMETA(DisplayName = "Rejected (locked)"),
	INVALID_REQUEST UMETA(DisplayName = "Invalid request"),
	NO_MANAGER UMETA(DisplayName = "No manager")
};

UENUM(BlueprintType)
enum class EGSAutoReverseMode : uint8
{
	DISABLED UMETA(DisplayName = "Disabled"),
	MID_AIR_THRESHOLD UMETA(DisplayName = "Mid-air threshold"),
	LANDING_IMPACT UMETA(DisplayName = "Landing impact"),
	MID_AIR_OR_LANDING UMETA(DisplayName = "Mid-air or landing")
};

UENUM(BlueprintType)
enum class EGSLandingResponseAction : uint8
{
	NONE UMETA(DisplayName = "None"),
	BOUNCE UMETA(DisplayName = "Bounce"),
	GRAVITY_REVERSED UMETA(DisplayName = "Gravity reversed"),
	SUPPRESSED UMETA(DisplayName = "Suppressed")
};

UENUM(BlueprintType)
enum class EGSGravitySwitchMode : uint8
{
	TOGGLE UMETA(DisplayName = "Toggle"),
	FORCE_NEGATIVE_Z UMETA(DisplayName = "Force -Z"),
	FORCE_POSITIVE_Z UMETA(DisplayName = "Force +Z")
};

USTRUCT(BlueprintType)
struct FGSImpactReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float EnergyJ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float RelativeNormalSpeedCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float RelativeNormalSpeedMps = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName SourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<AActor> OtherActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector ImpactNormal = FVector::UpVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	double TimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct FGSLandingReport
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	EGSLandingResponseAction Action = EGSLandingResponseAction::NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float ImpactSpeedCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float FallSpeedCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	float FallDistanceCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bGravityReversed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	double TimeSeconds = 0.0;
};

// Deterministic, non-stacking surface modifier. Highest priority wins.
USTRUCT(BlueprintType)
struct FGSSurfaceModifierSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityScaleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MaximumSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float ImpactEnergyMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float GravityAxisDragHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float TangentDragHz = 0.0f;
};

USTRUCT(BlueprintType)
struct FGSLandingModifierSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName ProfileId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bSuppressResponse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float NoResponseBelowImpactSpeedCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float BounceSpeedCm = 250.0f;

	// Impact speed at or above this value requests a gravity reverse instead of a bounce.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float AutoReverseAtSpeedCm = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float BounceTangentialRetention = 0.85f;
};

namespace GSGravity
{
	FORCEINLINE FVector PolarityToDirection(EGSGravityPolarity Polarity)
	{
		return Polarity == EGSGravityPolarity::POSITIVE_Z ? FVector(0.0, 0.0, 1.0) : FVector(0.0, 0.0, -1.0);
	}

	FORCEINLINE FVector PolarityToUp(EGSGravityPolarity Polarity)
	{
		return -PolarityToDirection(Polarity);
	}

	FORCEINLINE float CmToM(float Cm)
	{
		return Cm * 0.01f;
	}
}
