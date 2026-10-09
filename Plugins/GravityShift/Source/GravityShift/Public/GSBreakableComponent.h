// GravityShift v5 - impact energy driven breakable state (restorable, never destroyed on reset).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GSBreakableComponent.generated.h"

class UGSBreakProfile;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UPrimitiveComponent;
class UStaticMesh;

UCLASS(ClassGroup = (GravityShift), meta = (BlueprintSpawnableComponent, DisplayName = "GS Breakable"))
class GRAVITYSHIFT_API UGSBreakableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGSBreakableComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UPrimitiveComponent> TargetPrimitive = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UGSBreakProfile> BreakProfile = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bBreakable = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift")
	bool bBroken = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bOneHitBreakAboveThreshold = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bHideOwnerWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bDisableCollisionWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	bool bDisablePhysicsWhenBroken = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	FName RequiredSourceTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float MinimumImpactEnergyJ = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.01"))
	float MaximumHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float CurrentHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift", meta = (ClampMin = "0.0"))
	float DamageScalePerJ = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift")
	TObjectPtr<UStaticMesh> BrokenMesh = nullptr;

	/** 碎裂资产(Fracture Mode 烘的几何体集合)。填了就走真碎裂:原网格就地消失,
	 *  原地起一个 GeometryCollectionComponent 崩成碎块。留空 = 老行为(隐藏 + 换 BrokenMesh)。
	 *  碎块本身不需要额外设置——资产里照旧设成 Trigger 激活,由 BreakNow 来发令。
	 *  留空时按下面的约定路径自动找,所以关卡里通常不用填。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Fracture")
	TObjectPtr<UGeometryCollection> FractureCollection = nullptr;

	/** 约定的碎裂资产目录。留空 = /Game/GravityShift/Fracture/。
	 *  里面放 <网格名>_GC,例如 SM_LDI_Breakable_GC。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Fracture")
	FString FractureFolder = TEXT("/Game/GravityShift/Fracture");

	/** 运行时建的碎裂组件,只在"碎着"的时候存在;Repair() 销毁它 = 碎块消失。 */
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "GravityShift|Fracture")
	TObjectPtr<UGeometryCollectionComponent> FractureComponent = nullptr;

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void SetTargetPrimitive(UPrimitiveComponent* NewTarget);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void ApplyBreakProfile(UGSBreakProfile* NewProfile);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	float ApplyImpactEnergy(float EnergyJ, AActor* Instigator);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool BreakNow(AActor* Instigator, float EnergyJ);

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	bool Repair();

	UFUNCTION(BlueprintCallable, Category = "GravityShift")
	void CaptureInitialState();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "GravityShift")
	bool IsBroken() const;

	virtual void BeginPlay() override;

protected:
	bool bHasCapturedState = false;
	float InitialHealth = 100.0f;

	// BreakNow 进门时把"原状"记下来,Repair 照着还。在破坏那一刻取,
	// 而不是 BeginPlay 取 —— 方块的 Mesh/物理是 BeginPlay 之后才接上的,
	// 早取会记到空值,复原就成了瞎猜。
	TObjectPtr<UStaticMesh> MeshBeforeBreak = nullptr;
	bool bSimulatePhysicsBeforeBreak = false;
	ECollisionEnabled::Type CollisionBeforeBreak = ECollisionEnabled::QueryAndPhysics;

	bool SourceTagMatches(AActor* Instigator) const;

	/** 取碎裂资产:显式填了就用它,否则按约定路径找<网格名>_GC。找不到返回 null。 */
	UGeometryCollection* ResolveFractureCollection();

	/** 在 Owner 上临时建一个 GeometryCollectionComponent 并让它崩开。 */
	void SpawnFracture(UGeometryCollection* InCollection);
};
