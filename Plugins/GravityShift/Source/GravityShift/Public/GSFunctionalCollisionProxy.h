// GravityShift - explicit functional collision proxy for art-only levels.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSFunctionalCollisionProxy.generated.h"

class UBoxComponent;

UENUM(BlueprintType)
enum class EGSFunctionalCollisionRole : uint8
{
	Floor          UMETA(DisplayName = "Floor"),
	Wall           UMETA(DisplayName = "Wall"),
	Stair          UMETA(DisplayName = "Stair"),
	Guardrail      UMETA(DisplayName = "Guardrail"),
	DesignBoundary UMETA(DisplayName = "Design Boundary"),
	Review         UMETA(DisplayName = "Review")
};

/**
 * Deliberately invisible gameplay collision.  Use this instead of anonymous
 * hidden MetalBox actors when collision must stay separate from the art mesh.
 *
 * Important: the rolling ball is a PhysicsActor/PhysicsBody, so this proxy uses
 * normal WorldStatic blocking instead of a Pawn-only response.
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Functional Collision Proxy"))
class GRAVITYSHIFT_API AGSFunctionalCollisionProxy : public AActor
{
	GENERATED_BODY()

public:
	AGSFunctionalCollisionProxy();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Collision")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Collision")
	EGSFunctionalCollisionRole FunctionalRole = EGSFunctionalCollisionRole::Review;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Collision")
	FString SourceActorLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Collision")
	bool bEnabled = true;

	UFUNCTION(BlueprintCallable, Category = "GravityShift|Collision")
	void SetCollisionProxyEnabled(bool bNewEnabled);

	/** Replace a chunky AABB proxy with a thin floor slab at the original top face. */
	UFUNCTION(BlueprintCallable, Category = "GravityShift|Collision",
		meta = (ClampMin = "2.0", UIMin = "2.0"))
	void ConfigureAsFloorFromWorldBounds(const FVector& WorldCenter,
		const FVector& WorldExtent, float ThicknessCm = 12.0f);
};
