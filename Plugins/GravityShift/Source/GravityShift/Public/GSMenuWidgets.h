// GravityShift v5 - 主菜单与设置面板(纯 C++,无蓝图资产)。
//
// 为什么要用 C++ 建树:项目里一个 UMG 资产都没有,而 .uasset 没法用代码生成。
// 两个类的分工:
//   UGSMainMenuWidget   —— 外壳:全屏暗底 + 居中定宽竖排,内部切换"菜单"/"设置"两态。
//                          输入模式(UIOnly + 鼠标光标)由它独占负责。
//   UGSSettingsWidget   —— 只画设置内容(标题/滑块/数值/返回),**不带外壳**,设计上
//                          嵌在别人给的竖排里。将来做游戏内暂停菜单可以直接复用。
//
// 字体统一走 GEngine->GetMediumFont() —— 和 AGSGravityHUD 的 Canvas 中文字用的是同一个
// UFont,它带 DroidSansFallback 面,中文有字形。FCoreStyle 的默认字体是裸 ttf,中文会变方框。

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GSMenuWidgets.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGSOnSettingsBack);

// ---------------------------------------------------------------------------------
// 设置面板
// ---------------------------------------------------------------------------------

UCLASS(BlueprintType)
class GRAVITYSHIFT_API UGSSettingsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// "返回"被点。宿主靠这个把界面切回去 —— 面板不自己知道外面是什么。
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FGSOnSettingsBack OnBackRequested;

	// 把倍率实时应用到本地 Pawn(若有)。游戏内打开设置时立刻生效,不用等重开关卡。
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ApplyToLocalPawn(float Multiplier);

	// 从存档刷新滑块位置与数值文字。
	UFUNCTION(BlueprintCallable, Category = "Settings")
	void RefreshFromSave();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION()
	void HandleSliderChanged(float NewValue);

	// 拖动结束(松手)才落盘,不在 OnValueChanged 里写文件。
	UFUNCTION()
	void HandleSliderCommit();

	UFUNCTION()
	void HandleBackClicked();

private:
	void BuildTree();
	void SetValueLabel(float Value);

	UPROPERTY() TObjectPtr<UTextBlock> ValueText = nullptr;
	UPROPERTY() TObjectPtr<USlider> SensitivitySlider = nullptr;
	bool bBuilt = false;
};

// ---------------------------------------------------------------------------------
// 主菜单
// ---------------------------------------------------------------------------------

UCLASS(BlueprintType)
class GRAVITYSHIFT_API UGSMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// "开始游戏"要打开的关卡。默认游戏关卡,编辑器里可改。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GravityShift|Menu")
	FName GameLevelName = FName(TEXT("/Game/测试案例"));

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleStartGameClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleSettingsBack();

private:
	void BuildTree();
	void ShowSettings(bool bShow);

	UPROPERTY() TObjectPtr<UVerticalBox> MenuBox = nullptr;
	UPROPERTY() TObjectPtr<UGSSettingsWidget> SettingsPanel = nullptr;
	bool bBuilt = false;
};
