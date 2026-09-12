// GravityShift v5 - 用户设置的持久化存档。
//
// 只存"跨关卡 + 跨启动"都要保留的东西。目前只有鼠标灵敏度倍率。
// 读走 LoadOrCreate(),写走 SaveMultiplier(),别的地方不要直接碰 SaveGameToSlot。

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GSSettingsSaveGame.generated.h"

// 滑块范围和存档钳位共用这两个常数 —— 改一处即可,两边不允许不一致。
namespace GSSettingsLimits
{
	constexpr float MinSensitivity = 0.1f;
	constexpr float MaxSensitivity = 3.0f;
}

UCLASS(BlueprintType)
class GRAVITYSHIFT_API UGSSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static const FString SaveSlotName;
	static constexpr int32 UserIndex = 0;

	// 乘在 AGSRollingBallPawn 的 CameraYaw/PitchDegreesPerMouseUnit 上。1.0 = 不变。
	UPROPERTY(BlueprintReadWrite, Category = "Settings")
	float MouseSensitivityMultiplier = 1.0f;

	// 读存档。无存档/读取失败 → 返回倍率 1.0 的新实例。**永不返回 null**,调用方不用判空。
	// 载入时会钳位:存档文件被手改过的话,负倍率会让相机反向。
	UFUNCTION(BlueprintCallable, Category = "Settings")
	static UGSSettingsSaveGame* LoadOrCreate();

	// 钳位后写盘。越界值在这里收敛 —— 这是唯一的写入边界。
	UFUNCTION(BlueprintCallable, Category = "Settings")
	static void SaveMultiplier(float NewMultiplier);
};
