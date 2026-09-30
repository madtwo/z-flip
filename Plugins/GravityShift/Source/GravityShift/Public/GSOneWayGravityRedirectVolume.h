// GravityShift - gravity redirect volume (TWO-WAY by default) with local convex-arc assist.
// v7 CONTACT-AUTHORITATIVE FACE-CAPTURE patch (2026-09-29):
//   - touching the explicitly tagged bevel arc STARTS the pawn's FaceCapture state immediately, even if TriggerBox was missed;
//   - FaceCapture keeps the ball attached and rotates gravity continuously while travelling around the curve;
//   - the exit-normal threshold is a RELEASE condition, never an entry condition;
//   - legacy redirectors / other arc systems are outside this actor and are not modified.
// Patch purpose:
//   1) trigger a gravity redirect from a spatial volume without mesh-contact gates;
//   2) optionally keep the ball attached to the three convex corner meshes while it climbs over the lip;
//   3) keep the assist completely separate from gravity rotation and stop it as soon as a redirect owns the pawn.
//
// [2026-09-29 项目侧扩展] 双向 + 摆放方向驱动:
//   用户要求这个倒角圆弧"两边都行、两边都能换重力,而且像普通圆弧一样,摆到哪边就转哪边"。
//   这正是 UGSRedirectorComponent 自 v7 起的语义(入口面由"球当前重力"自动识别,出口就是另一面),
//   这里把同一条规则搬进体积:两面重力方向取本 Actor 的**局部轴**(LocalGravityADirection /
//   LocalGravityBDirection),旋转 Actor = 旋转转换效果;从哪一面进由球当前重力决定,两面都能进、都能转。
//   原单向行为(bBidirectional=false)与专家补丁的字段全部保留不变。

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
 * Designer-placeable, invisible gravity redirect volume. TWO-WAY by default since 2026-09-29.
 *
 * Triggering is polled instead of relying on overlap events. This makes the behavior independent of
 * collision channels / GenerateOverlapEvents and also supports a pawn whose gravity state changes
 * while it is already spatially inside the trigger.
 *
 * Two-way mode (bBidirectional, default ON): the ball may arrive on EITHER of the two perpendicular
 * faces this arc joins. The entry face is identified from the ball's CURRENT gravity; the exit is the
 * other face. Both faces are actor-local axes, so rotating the actor rotates the conversion -- place it
 * like an ordinary arc mesh. This is the same rule UGSRedirectorComponent has used since v7.
 *
 * In v7, bidirectional bevel arcs use AGSRollingBallPawn::BeginFaceCapture as the actual gravity
 * conversion state machine. Real contact with a tagged arc starts capture; the pawn then stays attached
 * and continuously rotates gravity toward the other face while travelling over the curve. The dedicated
 * v6 level configuration disables ArcAssist entirely so no second force system can compete with capture.
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

	/** Redirect destination. Wall -> world floor: NEGATIVE_Z. Used only in one-way mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect")
	EGSGravityDirection TargetGravityDirection = EGSGravityDirection::NEGATIVE_Z;

	// -------------------- 双向 / 摆放方向驱动 (2026-09-29) --------------------

	/**
	 * 双向模式(默认开)。球从**任一面**进入都会滑到另一面:入口面由"球当前重力"自动识别,出口就是另一面。
	 * 与 UGSRedirectorComponent 的"普通圆弧"语义一致。开时用下面两个局部轴;关时用 Entry/Target 两个枚举(原单向行为)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay")
	bool bBidirectional = true;

	/**
	 * 双向模式下"面 A"的重力方向,**本 Actor 的局部轴**(重力=球"向下"指向,不是面法线)。
	 * 旋转/摆放 Actor 即旋转转换效果。必须与面 B 垂直。默认 A = -Z(水平面上滚来的球)。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay")
	FVector LocalGravityADirection = FVector(0.0f, 0.0f, -1.0f);

	/**
	 * 双向模式下"面 B"的重力方向,同样是局部轴。默认 B = +X(球转到朝 +X 的那个竖直面上)。
	 * 例:想让球从水平面滚上"y 负方向的墙",把 Actor 绕 Z 转一下让 B 指到那个方向即可。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay")
	FVector LocalGravityBDirection = FVector(1.0f, 0.0f, 0.0f);

	/** 双向模式下,两面互相垂直的容差(|dot| ≤ 该值)。默认 0.10。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwoWayPerpendicularTolerance = 0.10f;

	/** Current gravity must match EntryGravityDirection this closely before the volume can fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EntryGravityMinDot = 0.90f;

	/** Explicit "already at target" guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetAlreadyMinDot = 0.90f;

	// -------------------- Two-way entry gates (v3) --------------------

	/**
	 * Two-way only. Require real motion + real curved-surface progress before switching gravity.
	 * This is the placement-independent replacement for shrinking TriggerBox to one hard-coded world-Y range.
	 * One-way mode deliberately ignores these gates to preserve the 2026-09-26 behavior.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate")
	bool bUseTwoWayEntryGates = true;

	/** Stationary balls on the crest must not fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0"))
	float TwoWayMinTriggerSpeedCm = 20.0f;

	/** Minimum velocity component along the curved surface toward the exit face. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0"))
	float TwoWayMinApproachSpeedCm = 20.0f;

	/** Require the pawn to still be supported by a surface when it enters the curved segment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate")
	bool bTwoWayRequireSupport = true;

	/** Small grace window for contact-state jitter at a convex crest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0"))
	float TwoWayMaxAirborneSeconds = 0.20f;

	/**
	 * Require a hit on ArcAssistSurfaceNameContains, not merely presence inside TriggerBox.
	 * This prevents side/bottom/empty-space entries from changing gravity.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate")
	bool bTwoWayRequireArcContact = true;

	/** Maximum allowed gap from ball surface to the configured arc surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0"))
	float TwoWayArcContactMarginCm = 20.0f;

	/** Reject side-wall normals that point too strongly along the rail/extrusion axis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwoWayMaxLateralNormalDot = 0.70f;

	/**
	 * Arc progress is measured from the contact normal: dot(ContactNormal, ExitUp).
	 * 0 = still on the entry tangent plane; 1 = already on the exit plane. Default 0.20 means
	 * the ball must be about 11.5 degrees into the curved segment before the instant gravity switch.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwoWayArcProgressMinDot = 0.20f;

	/**
	 * Legacy v5 threshold kept only so existing serialized maps load cleanly.
	 * The v6 bidirectional path does not use it to start, drive, or release FaceCapture.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwoWayArcExitNormalMinDot = 0.85f;

	/** Contact normal must stay inside the quadrant spanned by entry-up and exit-up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|EntryGate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TwoWayArcFaceNormalTolerance = 0.15f;

	// -------------------- v6 bevel FaceCapture --------------------

	/** Drive speed used only for a non-supported / hard capture entry. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|FaceCapture", meta = (ClampMin = "50.0"))
	float TwoWayFaceCaptureDriveSpeedCm = 700.0f;

	/** Inward acceleration that keeps the ball attached to the convex bevel during capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|FaceCapture", meta = (ClampMin = "0.0"))
	float TwoWayFaceCaptureStickAccelCm = 6000.0f;

	/** Release only after the live contact normal is close to the destination support normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|FaceCapture", meta = (ClampMin = "0.1", ClampMax = "0.999"))
	float TwoWayFaceCaptureExitNormalDot = 0.85f;

	/** Gentle-capture minimum tangent speed for a ball arriving from a supported surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|FaceCapture", meta = (ClampMin = "20.0"))
	float TwoWayFaceCaptureGroundMinSpeedCm = 280.0f;

	/** Gentle-capture maximum tangent speed; limits v^2/r separation on the convex bevel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|FaceCapture", meta = (ClampMin = "50.0"))
	float TwoWayFaceCaptureGroundMaxSpeedCm = 800.0f;

	/**
	 * Contact is allowed at zero/low approach speed. Reject only when the ball is clearly moving AWAY
	 * from the destination around the tangent. This prevents a direction gate from causing missed contact.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|OneWayRedirect|TwoWay|FaceCapture", meta = (ClampMin = "0.0"))
	float TwoWayFaceCaptureRejectAwaySpeedCm = 80.0f;

	/** Constant ride speed passed to AGSRollingBallPawn::BeginGravityRedirect. */
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

	/**
	 * v5 ARC-ONLY: when not None, ONLY actors carrying this tag may be used as curved assist/contact surfaces.
	 * This deliberately prevents unrelated curved actors or walls from being treated as this dedicated bevel arc.
	 * The level installer adds ZFlipTwoWayArc only to TwoWayArc_1/2/3 and does not edit other arc systems.
	 * Set to None only for legacy one-way maps that intentionally rely on the name-substring fallback.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	FName ArcAssistSurfaceTag = FName(TEXT("ZFlipTwoWayArc"));

	/**
	 * v5 ARC-ONLY: after this volume fires, suppress ArcAssist until the ball has left TriggerBox and the
	 * one-shot latch rearms. Without this guard, the new exit gravity is immediately reinterpreted as the
	 * opposite entry and the tangent guide can steer the ball back toward where it came from.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|ArcAssist")
	bool bSuppressArcAssistAfterRedirectUntilExit = true;

	/**
	 * Axis along the three-piece rail. In two-way mode this is an ACTOR-LOCAL axis, so rotating the
	 * volume rotates both gravity conversion and arc probing. In one-way mode it keeps the legacy
	 * world-space interpretation for backward compatibility.
	 */
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
	bool TryBeginTwoWayFaceCapture(AGSRollingBallPawn& Ball);
	bool PassesTwoWayEntryGates(AGSRollingBallPawn& Ball, const USphereComponent& BallSphere,
		const FVector& EntryGravity, const FVector& ExitGravity);
	void ClearOwnedRedirect(bool bKeepConsumedLatch);

	/** 双向模式:面 A / 面 B 的世界重力方向(由本 Actor 的旋转把局部轴转过去;忽略缩放)。 */
	FVector GetWorldGravityA() const;
	FVector GetWorldGravityB() const;
	FVector GetWorldArcAssistAxis() const;

	/**
	 * 双向模式入口识别:哪一面的重力与球当前重力匹配,就从那面进、出口是另一面。
	 * 两面都不匹配(球在别的姿态/空中)→ false,体积保持 fail-closed 不触发。
	 */
	bool ResolveTwoWayGravity(const FVector& CurrentGravity, FVector& OutEntry, FVector& OutExit) const;

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

	/** 最近一次识别出的入口/出口重力(世界方向)与出口枚举;仅用于日志与释放路径的兜底比较。 */
	FVector RuntimeEntryGravity = FVector::ZeroVector;
	FVector RuntimeExitGravity = FVector::ZeroVector;
	EGSGravityDirection RuntimeExitGravityEnum = EGSGravityDirection::NEGATIVE_Z;
	bool bRuntimeEntryResolved = false;

	/** Cached candidates for the local lip assist. */
	TArray<TWeakObjectPtr<AActor>> ArcAssistSurfaces;

	/** Keeps the chosen arc-tangent sign continuous around the crest. */
	FVector LastArcAssistTangent = FVector::ZeroVector;

	/** Debug counter resets each BeginPlay, which makes separate PIE sessions obvious in logs. */
	uint32 FireSerial = 0;
};
