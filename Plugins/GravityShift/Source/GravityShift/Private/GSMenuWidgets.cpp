#include "GSMenuWidgets.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Engine.h"
// FSlateFontInfo(const UObject*, ...) 要求 UFont 是完整类型才能做 UFont* → const UObject*
// 的派生转基类转换。少了这个 include 只会看到 TSharedPtr<FCompositeFont> 那个重载,报"不匹配"。
#include "Engine/Font.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Styling/CoreStyle.h"

#include "GSRollingBallPawn.h"
#include "GSSettingsSaveGame.h"

namespace
{
	// 面板文字统一用引擎 MediumFont:AGSGravityHUD 的 Canvas 中文用的就是它,带
	// DroidSansFallback 面。FCoreStyle 的默认字体是裸 ttf,中文会缺字形变方框。
	FSlateFontInfo PanelFont(int32 Size)
	{
		UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;
		return Font
			? FSlateFontInfo(Font, static_cast<float>(Size))
			: FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	UTextBlock* MakeText(UWidgetTree* Tree, const FString& Text, int32 FontSize, const FLinearColor& Color)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Block->SetText(FText::FromString(Text));
		Block->SetFont(PanelFont(FontSize));
		Block->SetColorAndOpacity(FSlateColor(Color));
		return Block;
	}

	// 竖排里加一行。UVerticalBox 没有行距属性,靠 slot 的 padding 拉开。
	void AddRow(UVerticalBox* Box, UWidget* Child, EHorizontalAlignment Align, float TopPadding)
	{
		if (UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(FMargin(0.0f, TopPadding, 0.0f, 0.0f));
			Slot->SetHorizontalAlignment(Align);
		}
	}

	UButton* MakeButton(UWidgetTree* Tree, const FString& Label, int32 FontSize)
	{
		UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		// 默认按钮底色是浅灰,标签必须深色,否则白字几乎看不见。
		Button->AddChild(MakeText(Tree, Label, FontSize, FLinearColor(0.05f, 0.05f, 0.06f)));
		return Button;
	}

	// 全屏暗底 + 居中定宽竖排。返回往里塞内容的竖排,并顺手把 WidgetTree 的根设好。
	UVerticalBox* BuildPanelShell(UWidgetTree* Tree, float ColumnWidth)
	{
		UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));

		UBorder* Backdrop = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Backdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.03f, 0.94f));
		Backdrop->SetHorizontalAlignment(HAlign_Center);
		Backdrop->SetVerticalAlignment(VAlign_Center);
		Backdrop->SetPadding(FMargin(24.0f));
		if (UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Backdrop))
		{
			Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			Slot->SetOffsets(FMargin(0.0f));
		}

		USizeBox* Column = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("Column"));
		Column->SetWidthOverride(ColumnWidth);
		Backdrop->AddChild(Column);

		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ColumnBox"));
		Column->AddChild(Box);

		Tree->RootWidget = Root;
		return Box;
	}

	// 菜单显示期间球不能被 WASD 推走、相机不能被鼠标转。输入是 Tick 轮询的,
	// FInputModeUIOnly 只管 Slate 那层拦不住它,必须显式关掉 Pawn 上的轮询开关。
	void SetLocalPawnNativeInput(UWorld* World, bool bEnabled)
	{
		APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
		AGSRollingBallPawn* Pawn = PC ? Cast<AGSRollingBallPawn>(PC->GetPawn()) : nullptr;
		if (Pawn)
		{
			Pawn->bEnableNativePollingInput = bEnabled;
		}
	}
}

// ---------------------------------------------------------------------------------
// 设置面板
// ---------------------------------------------------------------------------------

TSharedRef<SWidget> UGSSettingsWidget::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGSSettingsWidget::BuildTree()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree)
	{
		// 纯 C++ 的 UUserWidget 没有蓝图生成的 widget tree(它由 WidgetBlueprintGeneratedClass
		// 提供),自己建一个。
		Tree = WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	// 这里只建内容,不建外壳 —— 面板是嵌在宿主(目前是主菜单)的竖排里的。
	UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SettingsBox"));

	AddRow(Box, MakeText(Tree, TEXT("设置"), 40, FLinearColor::White), HAlign_Center, 0.0f);
	AddRow(Box, MakeText(Tree, TEXT("鼠标灵敏度"), 24, FLinearColor::White), HAlign_Center, 28.0f);

	ValueText = MakeText(Tree, TEXT("1.00x"), 24, FLinearColor(0.6f, 0.85f, 1.0f));
	AddRow(Box, ValueText, HAlign_Center, 10.0f);

	SensitivitySlider = Tree->ConstructWidget<USlider>(USlider::StaticClass(), TEXT("SensitivitySlider"));
	SensitivitySlider->SetMinValue(GSSettingsLimits::MinSensitivity);
	SensitivitySlider->SetMaxValue(GSSettingsLimits::MaxSensitivity);
	SensitivitySlider->OnValueChanged.AddDynamic(this, &UGSSettingsWidget::HandleSliderChanged);
	// 拖动结束才落盘。OnValueChanged 每帧都发,写在这里等于拖动时 60 次/秒写文件。
	SensitivitySlider->OnMouseCaptureEnd.AddDynamic(this, &UGSSettingsWidget::HandleSliderCommit);
	AddRow(Box, SensitivitySlider, HAlign_Fill, 14.0f);

	UButton* BackButton = MakeButton(Tree, TEXT("返回"), 28);
	BackButton->OnClicked.AddDynamic(this, &UGSSettingsWidget::HandleBackClicked);
	AddRow(Box, BackButton, HAlign_Fill, 34.0f);

	Tree->RootWidget = Box;

	// 建完就同步一次初值。不依赖 NativeConstruct 的时序,面板一出现就是存档里的值。
	RefreshFromSave();
}

void UGSSettingsWidget::RefreshFromSave()
{
	const float Value = UGSSettingsSaveGame::LoadOrCreate()->MouseSensitivityMultiplier;
	if (SensitivitySlider)
	{
		// USlider::SetValue 不发 OnValueChanged,不会绕回来再存一次。
		SensitivitySlider->SetValue(Value);
	}
	SetValueLabel(Value);
}

void UGSSettingsWidget::SetValueLabel(float Value)
{
	if (ValueText)
	{
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("%.2fx"), Value)));
	}
}

void UGSSettingsWidget::ApplyToLocalPawn(float Multiplier)
{
	APlayerController* PC = GetOwningPlayer();
	AGSRollingBallPawn* Pawn = PC ? Cast<AGSRollingBallPawn>(PC->GetPawn()) : nullptr;
	if (Pawn)
	{
		Pawn->MouseSensitivityMultiplier = Multiplier;
	}
}

void UGSSettingsWidget::HandleSliderChanged(float NewValue)
{
	SetValueLabel(NewValue);
	ApplyToLocalPawn(NewValue);
}

void UGSSettingsWidget::HandleSliderCommit()
{
	if (SensitivitySlider)
	{
		UGSSettingsSaveGame::SaveMultiplier(SensitivitySlider->GetValue());
	}
}

void UGSSettingsWidget::HandleBackClicked()
{
	// 可能没拖过滑块就直接返回,这里再存一次无害(值一样)。真正的意义是兜底:
	// 只要用户松手时 MouseCaptureEnd 因为任何原因没到,手感调了却不落盘会很难查。
	HandleSliderCommit();
	OnBackRequested.Broadcast();
}

// ---------------------------------------------------------------------------------
// 主菜单
// ---------------------------------------------------------------------------------

TSharedRef<SWidget> UGSMainMenuWidget::RebuildWidget()
{
	if (!bBuilt)
	{
		bBuilt = true;
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UGSMainMenuWidget::BuildTree()
{
	UWidgetTree* Tree = WidgetTree;
	if (!Tree)
	{
		Tree = WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}

	UVerticalBox* Shell = BuildPanelShell(Tree, 520.0f);

	MenuBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuBox"));
	AddRow(MenuBox, MakeText(Tree, TEXT("Z-Flip"), 64, FLinearColor::White), HAlign_Center, 0.0f);
	AddRow(MenuBox, MakeText(Tree, TEXT("重力翻转"), 22, FLinearColor(0.65f, 0.65f, 0.7f)), HAlign_Center, 8.0f);

	UButton* StartButton = MakeButton(Tree, TEXT("开始游戏"), 32);
	StartButton->OnClicked.AddDynamic(this, &UGSMainMenuWidget::HandleStartGameClicked);
	AddRow(MenuBox, StartButton, HAlign_Fill, 48.0f);

	UButton* SettingsButton = MakeButton(Tree, TEXT("设置"), 32);
	SettingsButton->OnClicked.AddDynamic(this, &UGSMainMenuWidget::HandleSettingsClicked);
	AddRow(MenuBox, SettingsButton, HAlign_Fill, 14.0f);

	AddRow(Shell, MenuBox, HAlign_Fill, 0.0f);

	SettingsPanel = CreateWidget<UGSSettingsWidget>(GetOwningPlayer(), UGSSettingsWidget::StaticClass());
	if (SettingsPanel)
	{
		SettingsPanel->OnBackRequested.AddDynamic(this, &UGSMainMenuWidget::HandleSettingsBack);
		SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
		AddRow(Shell, SettingsPanel, HAlign_Fill, 0.0f);
	}
}

void UGSMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeUIOnly());
		PC->bShowMouseCursor = true;
	}

	SetLocalPawnNativeInput(GetWorld(), false);
	ShowSettings(false);
}

void UGSMainMenuWidget::NativeDestruct()
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->bShowMouseCursor = false;
	}

	SetLocalPawnNativeInput(GetWorld(), true);

	Super::NativeDestruct();
}

void UGSMainMenuWidget::ShowSettings(bool bShow)
{
	if (MenuBox)
	{
		MenuBox->SetVisibility(bShow ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
	if (SettingsPanel)
	{
		if (bShow)
		{
			// 重开面板时同步一次,免得显示的是上次的旧值。
			SettingsPanel->RefreshFromSave();
		}
		SettingsPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UGSMainMenuWidget::HandleSettingsClicked()
{
	ShowSettings(true);
}

void UGSMainMenuWidget::HandleSettingsBack()
{
	ShowSettings(false);
}

void UGSMainMenuWidget::HandleStartGameClicked()
{
	// 关卡名带中文。源码是 UTF-8 无 BOM,万一编译器按 GBK 解了字面量,这行日志
	// 会直接打出乱码 —— 别删。
	UE_LOG(LogTemp, Log, TEXT("[GSMenu] 打开关卡 %s"), *GameLevelName.ToString());
	UGameplayStatics::OpenLevel(this, GameLevelName);
}
