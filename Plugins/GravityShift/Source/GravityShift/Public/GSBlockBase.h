// GravityShift v5 - one block class covering fixed, gravity, breaker and breakable recipes.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GSBlockBase.generated.h"

class UGSBlockProfile;
class UGSBreakableComponent;
class UGSGravityBodyComponent;
class UGSGridSnapComponent;
class UGSResettableComponent;
class UGSSurfaceReceiverComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "GS Block Base"))
class GRAVITYSHIFT_API AGSBlockBase : public AActor
{
	GENERATED_BODY()

public:
	AGSBlockBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UStaticMeshComponent> Mesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSGravityBodyComponent> GravityBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSSurfaceReceiverComponent> SurfaceReceiver = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSBreakableComponent> BreakableComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSResettableComponent> Resettable = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Components")
	TObjectPtr<UGSGridSnapComponent> GridSnapComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBlockProfile> BlockProfile = nullptr;

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

	/** 落地不弹:零回弹物理材质覆盖(Combine=Min),±Z 切换落下时稳稳停住。默认开。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bZeroBounceOnLand = true;

	/** 玩家推不动(2026-09-17 用户要求"可改变重力方块应该完全推不动",默认开):
	 *  两道保障——①质量抬到 ImmovableMassKg;②每帧把"垂直于方块自身重力轴"的速度分量清零,
	 *  玩家球撞上去连几厘米的位移都不会有(只靠质量方案仍会被高速球顶动几厘米)。
	 *  方块自己的 ±Z 切换/升起来照常:那是**沿重力轴**的质量无关加速度,两道保障都不碰这个方向。
	 *  需要能被推着走的方块,在实例上单独取消勾选即可。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bImmovableByPlayer = true;

	/** 推不动方案采用的质量(kg),玩家球撞上去几乎不产生位移。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "1.0"))
	float ImmovableMassKg = 2000.0f;

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
	FName BindingSourceWhiteboxId = NAME_None;

	// 新机制(2026-09-12):物体重力只剩 ±Z。false=向下掉,true=向上飘;
	// 由玩家准星(RMB 瞄准 + LMB)切换,加载时经 ApplyCurrentConfiguration 应用。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bGravityRises = false;

	// 非竖直重力(2026-09-13):空(0,0,0)=沿用老行为的世界 ±Z;填非零向量则按
	// **方块自身坐标系**的该轴定重力,bGravityRises 定正负(false=沿 -轴)。
	// 例:要重力朝自身 -X → 填 (1,0,0) 且 bGravityRises 保持 false。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FVector GravityAxisLocal = FVector::ZeroVector;

	// 悬浮待命(2026-10-10 星球坠落玩法):开局保持模拟物理但"不给重力"(GravityScale 按 0 计),
	// 像钉在半空一样纹丝不动;准星仍能锁定它(GravityBody->bGravityEnabled 保持 true)。
	// 首次被瞄准点击(ToggleGravityZ)时唤醒:恢复配置的 GravityScale,立即沿新方向走。
	// 典型配置:gravity_rises=true(初始朝上,不生效)+ 本开关开 → 点击后翻成 -Z 一路坠落。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bStartStaticUntilClicked = false;

	// 运行期标记:悬浮待命方块是否已被玩家点名唤醒。
	UPROPERTY(Transient, VisibleAnywhere, Category = "GravityShift")
	bool bActivatedByClick = false;

	/** 当前重力方向(世界空间)。零轴哨兵 = 老的世界 ±Z。 */
	FVector GetGravityAxisWorld() const;

	// 准星瞄准时的"边缘发光"覆盖材质(半透明菲涅尔);空则无高亮。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GravityShift")
	TObjectPtr<UMaterialInterface> AimGlowMaterial = nullptr;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyBlockProfile(UGSBlockProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetBlockMesh(UStaticMesh* NewMesh);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetSimulatingPhysics(bool bSimulate);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetAffectedByGravity(bool bAffected);

	// 清零线速度 + 角速度。配合 SetAffectedByGravity(false) 把方块"瞬间定住",而不是
	// 保留速度继续滑行。方块仍在模拟物理——重新启用重力后立刻恢复响应。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void FreezeMotion();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetCanBreakTargets(bool bCanBreak);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetBreakable(bool bIsBreakable);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyCurrentConfiguration();

	// 能否被准星改成 ±Z 重力:有 GravityBody 且正在模拟物理(固定块不可改)。
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool CanChangeGravity() const;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetGravityRises(bool bRises);

	// 掉下来 ↔ 升起来。返回切换后的 bGravityRises。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool ToggleGravityZ();

	// 准星瞄准高亮(边缘发光):换上 AimGlowMaterial,离开时恢复原材质。
	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetAimHighlight(bool bOn);

	virtual void BeginPlay() override;

	// 完全推不动(见 bImmovableByPlayer):每帧把垂直于方块自身重力轴的速度分量清零。
	// 只在 bImmovableByPlayer 时开 Tick(可推的方块零开销)。
	virtual void Tick(float DeltaSeconds) override;

protected:
	bool bAimHighlightOn = false;
};
