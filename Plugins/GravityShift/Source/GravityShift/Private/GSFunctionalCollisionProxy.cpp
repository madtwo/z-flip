#include "GSFunctionalCollisionProxy.h"

#include "Components/BoxComponent.h"
#include "Engine/CollisionProfile.h"

AGSFunctionalCollisionProxy::AGSFunctionalCollisionProxy()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("CollisionBox"));
	SetRootComponent(CollisionBox);
	CollisionBox->SetBoxExtent(FVector(50.0f));
	CollisionBox->SetCollisionObjectType(ECC_WorldStatic);
	CollisionBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBox->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	CollisionBox->SetGenerateOverlapEvents(false);
	CollisionBox->SetHiddenInGame(true);

	Tags.AddUnique(TEXT("GS_FunctionalCollisionProxy"));
}

void AGSFunctionalCollisionProxy::SetCollisionProxyEnabled(bool bNewEnabled)
{
	bEnabled = bNewEnabled;
	CollisionBox->SetCollisionEnabled(
		bEnabled ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
}

void AGSFunctionalCollisionProxy::ConfigureAsFloorFromWorldBounds(
	const FVector& WorldCenter, const FVector& WorldExtent, float ThicknessCm)
{
	const float HalfThickness = FMath::Max(1.0f, ThicknessCm * 0.5f);
	const FVector SafeExtent(
		FMath::Max(1.0f, WorldExtent.X),
		FMath::Max(1.0f, WorldExtent.Y),
		HalfThickness);

	// Keep the original top Z exactly; only collapse the volume downward into a slab.
	const float TopZ = WorldCenter.Z + WorldExtent.Z;
	SetActorLocation(FVector(WorldCenter.X, WorldCenter.Y, TopZ - HalfThickness));
	SetActorRotation(FRotator::ZeroRotator);
	CollisionBox->SetBoxExtent(SafeExtent, true);
	FunctionalRole = EGSFunctionalCollisionRole::Floor;
	SetCollisionProxyEnabled(true);
}
