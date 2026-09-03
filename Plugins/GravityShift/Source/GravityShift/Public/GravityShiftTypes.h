// GravityShift v6 - shared enums, structs and direction utilities.
// Contract: gravity is any of six axis directions (+/-X/Y/Z). The legacy
// EGSGravityPolarity (only +/-Z) survives for backwards-compatible callers.

#pragma once

#include "CoreMinimal.h"
#include "GravityShiftTypes.generated.h"

// Legacy 2-state polarity (Z axis only). Kept for backward-compatible BP/native
// callers; new code should use EGSGravityDirection. Only meaningful on the Z axis.
UENUM(BlueprintType)
enum class EGSGravityPolarity : uint8
{
	NEGATIVE_Z UMETA(DisplayName = "-Z (floor down)"),
	POSITIVE_Z UMETA(DisplayName = "+Z (ceiling down)")
};

// Six-axis gravity direction: which way "down" points.
UENUM(BlueprintType)
enum class EGSGravityDirection : uint8
{
	POSITIVE_X  UMETA(DisplayName = "+X"),
	NEGATIVE_X  UMETA(DisplayName = "-X"),
	POSITIVE_Y  UMETA(DisplayName = "+Y"),
	NEGATIVE_Y  UMETA(DisplayName = "-Y"),
	POSITIVE_Z  UMETA(DisplayName = "+Z"),
	NEGATIVE_Z  UMETA(DisplayName = "-Z")
};

// Gravity axis selector (used by the 1/2/3 keys and by allowed-axis lists).
UENUM(BlueprintType)
enum class EGSGravityAxis : uint8
{
	X UMETA(DisplayName = "X"),
	Y UMETA(DisplayName = "Y"),
	Z UMETA(DisplayName = "Z")
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
	REJECTED_DISABLED UMETA(DisplayName = "Rejected (direction disabled by level)"),
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
	// Legacy: only the Z axis exists here.
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

	// ---- six-direction helpers ----------------------------------------------

	FORCEINLINE FVector DirectionToVector(EGSGravityDirection Dir)
	{
		switch (Dir)
		{
		case EGSGravityDirection::POSITIVE_X: return FVector(1.0, 0.0, 0.0);
		case EGSGravityDirection::NEGATIVE_X: return FVector(-1.0, 0.0, 0.0);
		case EGSGravityDirection::POSITIVE_Y: return FVector(0.0, 1.0, 0.0);
		case EGSGravityDirection::NEGATIVE_Y: return FVector(0.0, -1.0, 0.0);
		case EGSGravityDirection::POSITIVE_Z: return FVector(0.0, 0.0, 1.0);
		default: return FVector(0.0, 0.0, -1.0); // NEGATIVE_Z
		}
	}

	FORCEINLINE FVector DirectionToUp(EGSGravityDirection Dir)
	{
		return -DirectionToVector(Dir);
	}

	FORCEINLINE EGSGravityDirection VectorToDirection(const FVector& Vec)
	{
		const FVector Abs(FMath::Abs(Vec.X), FMath::Abs(Vec.Y), FMath::Abs(Vec.Z));
		if (Abs.X >= Abs.Y && Abs.X >= Abs.Z)
		{
			return Vec.X >= 0.0f ? EGSGravityDirection::POSITIVE_X : EGSGravityDirection::NEGATIVE_X;
		}
		if (Abs.Y >= Abs.Z)
		{
			return Vec.Y >= 0.0f ? EGSGravityDirection::POSITIVE_Y : EGSGravityDirection::NEGATIVE_Y;
		}
		return Vec.Z >= 0.0f ? EGSGravityDirection::POSITIVE_Z : EGSGravityDirection::NEGATIVE_Z;
	}

	FORCEINLINE EGSGravityDirection FlipDirection(EGSGravityDirection Dir)
	{
		switch (Dir)
		{
		case EGSGravityDirection::POSITIVE_X: return EGSGravityDirection::NEGATIVE_X;
		case EGSGravityDirection::NEGATIVE_X: return EGSGravityDirection::POSITIVE_X;
		case EGSGravityDirection::POSITIVE_Y: return EGSGravityDirection::NEGATIVE_Y;
		case EGSGravityDirection::NEGATIVE_Y: return EGSGravityDirection::POSITIVE_Y;
		case EGSGravityDirection::POSITIVE_Z: return EGSGravityDirection::NEGATIVE_Z;
		default: return EGSGravityDirection::POSITIVE_Z; // NEGATIVE_Z
		}
	}

	FORCEINLINE EGSGravityAxis GetAxisFromDirection(EGSGravityDirection Dir)
	{
		switch (Dir)
		{
		case EGSGravityDirection::POSITIVE_X:
		case EGSGravityDirection::NEGATIVE_X: return EGSGravityAxis::X;
		case EGSGravityDirection::POSITIVE_Y:
		case EGSGravityDirection::NEGATIVE_Y: return EGSGravityAxis::Y;
		default: return EGSGravityAxis::Z;
		}
	}

	FORCEINLINE bool IsPositive(EGSGravityDirection Dir)
	{
		return Dir == EGSGravityDirection::POSITIVE_X
			|| Dir == EGSGravityDirection::POSITIVE_Y
			|| Dir == EGSGravityDirection::POSITIVE_Z;
	}

	FORCEINLINE EGSGravityDirection GetPositiveDirection(EGSGravityAxis Axis)
	{
		switch (Axis)
		{
		case EGSGravityAxis::X: return EGSGravityDirection::POSITIVE_X;
		case EGSGravityAxis::Y: return EGSGravityDirection::POSITIVE_Y;
		default: return EGSGravityDirection::POSITIVE_Z;
		}
	}

	FORCEINLINE EGSGravityDirection GetNegativeDirection(EGSGravityAxis Axis)
	{
		switch (Axis)
		{
		case EGSGravityAxis::X: return EGSGravityDirection::NEGATIVE_X;
		case EGSGravityAxis::Y: return EGSGravityDirection::NEGATIVE_Y;
		default: return EGSGravityDirection::NEGATIVE_Z;
		}
	}

	// An empty AllowedAxes array means "all axes allowed".
	FORCEINLINE bool IsAxisAllowed(EGSGravityAxis Axis, const TArray<EGSGravityAxis>& AllowedAxes)
	{
		return AllowedAxes.Num() == 0 || AllowedAxes.Contains(Axis);
	}

	FORCEINLINE FString GetDirectionDisplayName(EGSGravityDirection Dir)
	{
		switch (Dir)
		{
		case EGSGravityDirection::POSITIVE_X: return TEXT("+X");
		case EGSGravityDirection::NEGATIVE_X: return TEXT("-X");
		case EGSGravityDirection::POSITIVE_Y: return TEXT("+Y");
		case EGSGravityDirection::NEGATIVE_Y: return TEXT("-Y");
		case EGSGravityDirection::POSITIVE_Z: return TEXT("+Z");
		default: return TEXT("-Z");
		}
	}

	FORCEINLINE FString GetAxisDisplayName(EGSGravityAxis Axis)
	{
		switch (Axis)
		{
		case EGSGravityAxis::X: return TEXT("X");
		case EGSGravityAxis::Y: return TEXT("Y");
		default: return TEXT("Z");
		}
	}

	// Legacy sign-only projection of a direction onto the old 2-state polarity.
	// Only preserved for callers of the pre-six-direction API; do not treat the
	// result as a real +/-Z meaning when the current axis is X or Y.
	FORCEINLINE EGSGravityPolarity DirectionToLegacyPolarity(EGSGravityDirection Dir)
	{
		return IsPositive(Dir) ? EGSGravityPolarity::POSITIVE_Z : EGSGravityPolarity::NEGATIVE_Z;
	}
}
