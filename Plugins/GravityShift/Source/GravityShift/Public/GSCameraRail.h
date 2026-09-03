// GravityShift v6 - camera rail: an invisible track the player camera rides.
// The axis is the actor's local Z. The camera slides along it with the ball like
// a ring on a rod; its position inside the cross-section stays fixed during play.
// Place one per level segment and rotate the actor so local Z points the way the
// camera should look.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSCameraRail.generated.h"

UCLASS(Blueprintable, meta = (DisplayName = "GS Camera Rail"))
class GRAVITYSHIFT_API AGSCameraRail : public AActor
{
	GENERATED_BODY()

public:
	AGSCameraRail();

	// Full track length along local Z, centered on the actor location.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Rail", meta = (ClampMin = "1.0"))
	float RailLength = 2000.0f;

	// Base view direction: false looks along +local Z, true along -local Z.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Rail")
	bool bLookAlongNegativeAxis = false;

	// Lifts the camera this far off the axis along world up (perpendicular part).
	// 0 keeps the camera ring exactly on the axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Rail", meta = (ClampMin = "0.0"))
	float CrossOffsetHeightCm = 0.0f;

	UFUNCTION(BlueprintPure, Category = "GravityShift|Rail")
	FVector GetAxisDirection() const { return GetActorQuat().GetAxisZ(); }

	// Signed position along the axis; 0 is the actor origin.
	UFUNCTION(BlueprintPure, Category = "GravityShift|Rail")
	float ProjectAlongAxis(const FVector& Point) const
	{
		return FVector::DotProduct(Point - GetActorLocation(), GetAxisDirection());
	}

	UFUNCTION(BlueprintPure, Category = "GravityShift|Rail")
	float DistanceToAxis(const FVector& Point) const;

	// True when the point projects within the rail extent plus margin.
	UFUNCTION(BlueprintPure, Category = "GravityShift|Rail")
	bool IsPointInZone(const FVector& Point, float EndMarginCm = 0.0f) const
	{
		return FMath::Abs(ProjectAlongAxis(Point)) <= RailLength * 0.5f + EndMarginCm;
	}
};
