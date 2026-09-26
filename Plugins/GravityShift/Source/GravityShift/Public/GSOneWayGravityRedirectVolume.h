// GravityShift - one-way wall -> floor redirect volume with local convex-arc assist.
// Patch purpose:
//   1) trigger a one-way gravity redirect from a spatial volume without mesh-contact gates;
//   2) optionally keep the ball attached to the three convex corner meshes while it climbs over the lip;
//   3) keep the assist completely separate from gravity rotation and stop it as soon as a redirect owns the pawn.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GravityShiftTypes.h"
#include "GSOneWayGravityRedirectVolume.generated.h"

class UBoxComponent;
class USphereComponent;
class AActor;
class AGSRollingBallPawn;

/**
 * Designer-placeable, invisible one-way redirect volume.
 *
 * Triggering is polled instead of relying on overlap events. This makes the behavior independent of
 * collision channels / GenerateOverlapEvents and also supports a pawn whose gravity state changes
 * while it is already spatially inside the trigger.
 *
 * The optional ArcAssistBox is NOT another gravity redirect. It only applies a local inward force and
 * a small tangent guide while the pawn still has the configured wall gravity. It searches only the
 * named curved meshes. As soon as BeginGravityRedirect starts (or FaceCapture owns the pawn), the
 * assist stops in the same tick so the two mechanisms never fight over velocity.
 */
UCLASS(BlueprintType, Blueprintable)
class GRAVITYSHIFT_API AGSOneWayGravityRedirectVolume : public AActor
{
	GENERATED_BODY()

public:
	AGSOneWayGravityRedirectVolume();

	/** Invisible redirect trigger. Collision is disabled; it is used as an oriented spatial box only. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|OneWayRedirect")
	TObjectPtr<UBoxComponent> TriggerBox;

	/**
	 * Invisible local assist region around the convex lip. It never blocks the ball.
	 * Default relative placement is tuned for the reported Re_Blockout arc when TriggerBox is at
	 * (3550,-1330,-400): assist center becomes approximately (3550,-1248,-560).
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|ArcAssist")
	TObjectPtr<UBoxComponent> ArcAssistBox;

	/** The ONLY gravity state allowed to enter this redirect. Reported wall: NEGATIVE_Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect")
	EGSGravityDirection EntryGravityDirection = EGSGravityDirection::NEGATIVE_Y;

	/** Redirect destination. Wall -> world floor: NEGATIVE_Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect")
	EGSGravityDirection TargetGravityDirection = EGSGravityDirection::NEGATIVE_Z;

	/** Current gravity must match EntryGravityDirection this closely before the volume can fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntryGravityMinDot = 0.90f;

	/** Explicit "already at target" guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetAlreadyMinDot = 0.90f;

	/** Constant ride speed passed to AGSRollingBallPawn::BeginGravityRedirect. No speed gate is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "50.0"))
	float RideSpeedCm = 800.0f;

	/** Travel distance used by the pawn to animate gravity rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "10.0"))
	float RidePathLengthCm = 260.0f;

	/** Progress threshold for releasing control. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.90", ClampMax = "1.0"))
	float CompletionProgress = 0.999f;

	/**
	 * Extra release guard: before this volume calls EndGravityRedirect, the pawn's active gravity must
	 * also be close to our intended target. This makes the existing best-effort ownership latch safer
	 * if another system interrupts and starts a different redirect between ticks.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.90", ClampMax = "1.0"))
	float CompletionTargetMinDot = 0.995f;

	/**
	 * If true, spatial triggering behaves like sphere-vs-box overlap by expanding TriggerBox by the ball
	 * radius. If false, the BALL CENTER must enter the box. For the reported thin layer above the platform,
	 * false is intentional: it prevents firing roughly one ball radius before the center crosses the crest.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect")
	bool bExpandTriggerByBallRadius = false;

	/** Extra spatial margin in cm. Applied whether or not ball-radius expansion is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.0"))
	float TriggerPaddingCm = 0.0f;

	/** Master switch for level-side A/B testing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect")
	bool bEnabled = true;

	// -------------------- Convex-arc assist --------------------

	/** Local lip assist. This does NOT rotate gravity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	bool bArcAssistEnabled = true;

	/** Assist only while gravity still matches this wall state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	EGSGravityDirection ArcAssistGravityDirection = EGSGravityDirection::NEGATIVE_Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ArcAssistGravityMinDot = 0.90f;

	/**
	 * Runtime actor-name substring used to discover the three curved mesh actors once at BeginPlay.
	 * "Blockout_Corner_Curved" matches Blockout_Corner_Curved / 2 / 3 in the supplied level.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	FString ArcAssistSurfaceNameContains = TEXT("Blockout_Corner_Curved");

	/** Axis along the three-piece rail. Probe rays are cast in the plane perpendicular to this axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	FVector ArcAssistAxis = FVector(1.0f, 0.0f, 0.0f);

	/** Number of rays in the 360-degree probe fan around the ball. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "8", ClampMax = "64"))
	int32 ArcAssistProbeRays = 24;

	/** Probe distance outside the ball radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "10.0"))
	float ArcAssistProbeReachCm = 110.0f;

	/** Base inward acceleration while a configured curved surface is found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0"))
	float ArcAssistBaseAccelCm = 1800.0f;

	/** Approximate ball-center path radius around the 100cm corner (100 + ~48cm ball radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "10.0"))
	float ArcAssistPathRadiusCm = 148.0f;

	/** Multiplier on v^2/r centripetal demand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float ArcAssistCentripetalScale = 1.10f;

	/** Additional inward acceleration per cm of positive surface gap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0"))
	float ArcAssistGapGain = 35.0f;

	/** Safety cap for inward acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0"))
	float ArcAssistMaxAccelCm = 9000.0f;

	/**
	 * Minimum arc-tangent speed while the helper is actually seeing one of the three curved meshes.
	 * This carries an upward-moving ball around the crest without changing gravity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0"))
	float ArcAssistMinTangentSpeedCm = 420.0f;

	/** Cap for the tangent catch-up acceleration used to reach ArcAssistMinTangentSpeedCm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist", meta = (ClampMin = "0.0"))
	float ArcAssistMaxGuideAccelCm = 4500.0f;

	/**
	 * Maximum velocity component allowed directly away from the curved surface. Set negative to disable.
	 * The default is intentionally small but non-zero so collision response is not made perfectly rigid.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	float ArcAssistMaxOutwardSpeedCm = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bDebugLog = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bArcAssistDebugLog = false;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	bool IsBallInsideBox(const UBoxComponent& Box, const USphereComponent& BallSphere, bool bExpandByBallRadius, float ExtraPaddingCm) const;
	bool TryBeginRedirect(AGSRollingBallPawn& Ball);
	void ClearOwnedRedirect(bool bKeepConsumedLatch);

	void RefreshArcAssistSurfaces();
	bool FindArcAssistSurfaceHit(const USphereComponent& BallSphere, FHitResult& OutHit, FVector& OutInwardDirection) const;
	void UpdateArcAssist(AGSRollingBallPawn& Ball, float DeltaSeconds);
	void ResetArcAssistState();

	/** The pawn instance we are observing; replacement/reset clears occupancy state. */
	TWeakObjectPtr<AGSRollingBallPawn> ObservedBall;

	/** Pawn whose redirect was started by this volume. */
	TWeakObjectPtr<AGSRollingBallPawn> ActiveBall;

	/** True while we are responsible for releasing the redirect we started. */
	bool bOwnsRedirect = false;

	/** One-shot latch for the current stay inside TriggerBox. */
	bool bConsumedUntilExit = false;

	/** Cached candidates for the local lip assist. */
	TArray<TWeakObjectPtr<AActor>> ArcAssistSurfaces;

	/** Keeps the chosen arc-tangent sign continuous around the crest. */
	FVector LastArcAssistTangent = FVector::ZeroVector;

	/** Debug counter resets each BeginPlay, which makes separate PIE sessions obvious in logs. */
	uint32 FireSerial = 0;
};
