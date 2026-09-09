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
class UGSRailCameraComponent;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSRailCameraComponent> RailCamera = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBallProfile> BallProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float StopTorqueAcceleration = 160.0f;

	// WASD 驱动的平面加速度(cm/s²,质量无关)。终端速度 ≈ 此值/切向拖拽(0.35)。
	// 只作用于支撑面上的 WASD 路径,重力/掉落/反弹不受影响。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float DriveAccelerationCm = 400.0f;

	// 松键刹车的平面速度衰减率(Hz):e^{-Hz·dt},~3 = 1 秒内基本停稳。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float ReleaseBrakeHz = 3.0f;

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

	// CameraPivot location is written in world space and does NOT inherit the
	// physics ball's intra-frame displacement (that bypasses all camera smoothing
	// and shows as small instant jitter). Turn off only to A/B the old behaviour.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bUseAbsoluteCameraLocation = true;

	// Per-frame rail-camera debug log (pivot pre-write position / ball / target).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bRailCamDebugLog = false;

	// Per-frame fallback-camera debug log (ball / pivot / actual arm length / lift
	// scale / aim pitch) — for diagnosing stair-climb camera shake.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bFallbackCamDebugLog = false;

	// Debug: hold full forward drive every tick (stair-climb camera shake repro).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Debug")
	bool bDebugAutoDriveForward = false;

	// Interact moved from E to F: E is now the camera-distance key (Q/E).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey InteractKey = EKeys::F;

	// Dismisses a center-screen pickup message (see ShowMessageAndLock).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey DismissMessageKey = EKeys::SpaceBar;

	// Player camera-distance keys: Q pulls the rail camera closer, E sends it back.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey TrailCloserKey = EKeys::Q;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey TrailFartherKey = EKeys::E;

	// Player speed keys: O slows the WASD drive down, P speeds it up (live).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey SpeedDownKey = EKeys::O;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey SpeedUpKey = EKeys::P;

	// O/P 每次按的步长与范围(终端速度 ≈ DriveAccelerationCm×0.28)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "1.0"))
	float DriveAdjustStepCm = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float DriveMinCm = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float DriveMaxCm = 7200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey ResetKey = EKeys::R;

	// Set-axis keys: snap gravity to the positive direction of the pressed axis.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey AxisSetXKey = EKeys::One;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey AxisSetYKey = EKeys::Two;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input")
	FKey AxisSetZKey = EKeys::Three;

	// How long the "X轴不可用" hint stays on screen after a disallowed 1/2/3 press.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Input", meta = (ClampMin = "0.0"))
	float AxisHintLifetimeSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bAllowAirControl = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement")
	bool bClampPlanarSpeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float RollTorqueAcceleration = 44.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float MaximumPlanarSpeedCm = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Movement", meta = (ClampMin = "0.0"))
	float AirControlAccelerationCm = 900.0f;

	// 无导轨相机滚转锁世界竖直(与导轨相机同一原则):G 翻转只平移跟球、画面不颠倒
	//(颠倒视角会晕 3D,用户定案)。设 true 恢复旧的随重力翻转行为。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	bool bCameraFlipsWithGravity = false;

	// 角度微调:轴向瞄准"球 + 支撑面上方 AimUp"——抬升侧翻转时自动把球压回画面
	// 下三分之一(地板态微俯/天花板态微抬);AimUp = Base + Swing×dot(抬升方向,支撑上)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraAimUpBaseCm = 142.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraAimUpSwingCm = -7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.01"))
	float CameraFlipDurationSeconds = 0.35f;

	// 无导轨第三人称:枢轴沿当前"上"轴抬高,让球出现在画面下三分之一(常见第三人称
	// 取景,贴近后期导轨相机的观感);0 = 球居中(旧行为)。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraPivotLiftHeightCm = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraFollowInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraYawDegreesPerMouseUnit = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraPitchDegreesPerMouseUnit = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0", ClampMax = "89.0"))
	float MaximumCameraPitchDegrees = 70.0f;

	// 俯仰默认 0:配合枢轴抬高,球落在画面下三分之一;鼠标仍可自由俯仰。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraPitchDegrees = 0.0f;

	// 已被 CameraAimHeading 取代(保留是为了兼容旧序列化实例,不再参与计算)。
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera")
	float CameraYawDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float CameraArmLengthCm = 700.0f;

	// 相机臂长放长速度(自建探针命中→立即收短;连续无命中 ArmExtendHoldSeconds 后才按
	// 此速度放长,台阶边缘探针逐帧翻转被去弹,相机不再弹跳)。越大回位越快。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.1"))
	float ArmLengthInterpSpeed = 5.0f;

	// "连续无命中"需要保持多久才允许放长臂(去弹窗口)。爬楼梯时探针命中/无命中以
	// ~1Hz 交替,窗口短于无命中周期相机就会来回抽——0.7s 足够盖住,又不会让离墙后的
	// 拉远显得拖沓。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Camera", meta = (ClampMin = "0.0"))
	float ArmExtendHoldSeconds = 0.7f;

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

	// Requests an arbitrary gravity direction (script/level use).
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	EGSGravityRequestResult RequestGravityDirection(EGSGravityDirection NewDirection, bool bForce);

	UFUNCTION(BlueprintPure, Category = "GravityShift")
	EGSGravityDirection GetCurrentGravityDirection() const;

	// Transient "X axis unavailable" feedback (driven by 1/2/3 on disallowed axes).
	UFUNCTION(BlueprintPure, Category = "GravityShift")
	bool IsAxisHintActive() const;

	// 无导轨相机枢轴抬升方向(瞬时状态,非反射):跟随"支撑面外侧"(=−重力)平滑摆动,
	// 让天花板/墙面态的相机始终在球靠房间的一侧、球保持下三分之一取景。
	FVector CameraLiftDirection = FVector::UpVector;

	// 无导轨相机枢轴位置平滑(瞬时状态,非反射;速率=Profile 的 CameraFollowInterpSpeed)。
	FVector SmoothedPivotLocation = FVector::ZeroVector;
	bool bPivotSmoothed = false;

	// 平滑后的实测臂长(探针塌缩会逐帧翻转,直接喂 LiftScale 会让枢轴高度抽动)。
	float SmoothedArmLengthCm = -1.0f;

	// 探针"连续无命中"计时:命中立即收短、连续无命中 0.25s 才放长(去弹翻转)。
	float ProbeClearSeconds = 0.0f;

	UFUNCTION(BlueprintPure, Category = "GravityShift")
	FString GetAxisHintText() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool TryInteract();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FText GetCurrentInteractionText() const;

	// Shows Message center-screen and suppresses gameplay input until the player
	// presses DismissMessageKey. Used by pickups/key hints.
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ShowMessageAndLock(const FText& Message);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void DismissPendingMessage();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsMessageLocked() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FText GetPendingMessage() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	FKey GetMessageDismissKey() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ResetToCheckpoint();

	// O/P 键:游戏内实时调 WASD 驱动力(Direction −1=减速 / +1=加速,步长/范围见
	// DriveAdjustStepCm 等参数;终端速度 ≈ DriveAccelerationCm×0.28)。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void AdjustDriveSpeed(float Direction);

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
	bool bRailCamActive = false;
	bool bFlipKeyWasDown = false;
	bool bInteractKeyWasDown = false;
	bool bResetKeyWasDown = false;
	bool bTrailCloserKeyWasDown = false;
	bool bTrailFartherKeyWasDown = false;
	bool bSpeedDownKeyWasDown = false;
	bool bSpeedUpKeyWasDown = false;
	bool bAxisSetXWasDown = false;
	bool bAxisSetYWasDown = false;
	bool bAxisSetZWasDown = false;
	bool bDismissKeyWasDown = false;

	bool bInputLocked = false;
	FText PendingMessage;

	float AxisHintExpireTime = -1.0f;
	FString AxisHintText;

	FVector GetActiveGravityDirection() const;
	FQuat BuildCameraRotation(const FVector& UpVector, float AdditionalPitchDegrees = 0.0f) const;

	// 世界航向(单位向量):无导轨相机的水平瞄准方向。鼠标转向绕当前重力上轴旋转它,
	// G 翻转(上轴 ±Z 互换)不改变航向 → 视角翻转后仍对准同一个世界方向。
	FVector CameraAimHeading = FVector::ForwardVector;
	void UpdateCamera(float DeltaSeconds);
	void ApplyMovement(float DeltaSeconds);
	void PollNativeInput();
	void HandleFlipPressed();
	void HandleSetGravityAxis(EGSGravityAxis Axis);
	void ShowAxisDisabledHint(EGSGravityAxis Axis);
	AActor* FindBestInteractable() const;

	UFUNCTION()
	void HandleGravityChanged(EGSGravityPolarity NewPolarity, FVector GravityDirection, int32 Revision, EGSGravityChangeReason Reason);
};
