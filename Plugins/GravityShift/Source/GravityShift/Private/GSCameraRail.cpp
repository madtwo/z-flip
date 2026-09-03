#include "GSCameraRail.h"

AGSCameraRail::AGSCameraRail()
{
	PrimaryActorTick.bCanEverTick = false;

	// Purely a data marker: a scene root renders nothing, has no collision and
	// never appears in game — matching the designer's reference cylinder.
	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
}

float AGSCameraRail::DistanceToAxis(const FVector& Point) const
{
	const FVector Closest = GetActorLocation() + GetAxisDirection() * ProjectAlongAxis(Point);
	return FVector::Dist(Closest, Point);
}
