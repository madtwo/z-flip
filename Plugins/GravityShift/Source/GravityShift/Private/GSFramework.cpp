#include "GSFramework.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

#include "GSGravityManager.h"
#include "GSLandingResponseComponent.h"
#include "GSProfiles.h"
#include "GSRollingBallPawn.h"
#include "GSWorldState.h"

// ---------------------------------------------------------------------------------
// Game mode
// ---------------------------------------------------------------------------------

AGSGravityGameMode::AGSGravityGameMode()
{
	DefaultPawnClass = AGSRollingBallPawn::StaticClass();
	HUDClass = AGSGravityHUD::StaticClass();

	GravityManagerClass = AGSGravityManager::StaticClass();
	WorldStateManagerClass = AGSWorldStateManager::StaticClass();
}

void AGSGravityGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoSpawnCoreManagers)
	{
		EnsureCoreManagers();
	}
}

void AGSGravityGameMode::EnsureCoreManagers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!AGSGravityManager::FindGravityManager(this) && GravityManagerClass)
	{
		AGSGravityManager* Manager = World->SpawnActor<AGSGravityManager>(GravityManagerClass, FTransform::Identity);
		Manager->SetActorLabel(TEXT("GravityShift_Manager"));
		UE_LOG(LogTemp, Log, TEXT("[GravityShift] game mode spawned gravity manager"));
	}

	if (!AGSWorldStateManager::FindWorldStateManager(this) && WorldStateManagerClass)
	{
		AGSWorldStateManager* StateManager = World->SpawnActor<AGSWorldStateManager>(WorldStateManagerClass, FTransform::Identity);
		StateManager->SetActorLabel(TEXT("GravityShift_WorldState"));
		UE_LOG(LogTemp, Log, TEXT("[GravityShift] game mode spawned world state manager"));
	}

	// Apply the world-state manager's level gravity config as early as possible so
	// the ball spawns with the level's intended direction (the manager may spawn
	// after level-placed actors, hence this call in addition to WSM BeginPlay).
	if (AGSWorldStateManager* StateManager = AGSWorldStateManager::FindWorldStateManager(this))
	{
		StateManager->ApplyLevelGravityConfig();
	}
}

AActor* AGSGravityGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// Keep spawn selection deterministic when Main streams Level1/Level2 and all of
	// them contain their own PlayerStart.  Each start is tagged with the persistent
	// map it belongs to: Main / Level1 / Level2 / ... .
	// GetCurrentLevelName(..., true) strips the PIE prefix (UEDPIE_0_, etc.).
	const FString PersistentMapName = UGameplayStatics::GetCurrentLevelName(this, true);
	const FName DesiredTag(*PersistentMapName);

	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		APlayerStart* Start = *It;
		if (IsValid(Start) && Start->PlayerStartTag == DesiredTag)
		{
			UE_LOG(LogTemp, Log, TEXT("[GravityShift] PlayerStart '%s' selected for map '%s'"),
				*Start->GetName(), *PersistentMapName);
			return Start;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[GravityShift] No PlayerStart tagged '%s'; falling back to GameModeBase selection"),
		*PersistentMapName);
	return Super::ChoosePlayerStart_Implementation(Player);
}

void AGSGravityGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	AGSRollingBallPawn* Ball = NewPlayer ? Cast<AGSRollingBallPawn>(NewPlayer->GetPawn()) : nullptr;
	if (Ball)
	{
		if (DefaultBallProfile)
		{
			Ball->ApplyBallProfile(DefaultBallProfile);
		}
		Ball->RefreshSystemReferences();

		// Re-assert the level gravity config once the pawn is up. By now every
		// actor BeginPlay (including a game-mode-spawned manager) has run, so this
		// settles the ordering where the manager's profile default would otherwise
		// clobber the level default.
		if (AGSWorldStateManager* StateManager = AGSWorldStateManager::FindWorldStateManager(NewPlayer))
		{
			StateManager->ApplyLevelGravityConfig();
		}
	}
}

// ---------------------------------------------------------------------------------
// HUD
// ---------------------------------------------------------------------------------

AGSGravityHUD::AGSGravityHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AGSGravityHUD::SetHUDVisible(bool bVisible)
{
	bHUDVisible = bVisible;
}

void AGSGravityHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!bHUDVisible || !Canvas || !GEngine)
	{
		return;
	}

	AGSRollingBallPawn* Ball = PlayerOwner ? Cast<AGSRollingBallPawn>(PlayerOwner->GetPawn()) : nullptr;
	if (!Ball)
	{
		return;
	}

	// While a pickup/key/door message holds the player's input, draw it center-screen
	// and suppress the corner debug block so the text reads cleanly.
	if (Ball->IsMessageLocked())
	{
		// ⚠ lambda 里不能按名捕获成员(Canvas 是 AHUD 的非静态成员 ⇒ C2327/C2065):
		// 先落成局部指针,再按值捕获这两个局部变量。
		UCanvas* const DrawCanvas = Canvas;
		// 线索用**大号字**(用户:拾取的字体大一点)。
		UFont* const DrawFont = GEngine->GetLargeFont();
		const FColor MainColor = TextColor.ToFColor(true);
		// 线索压在场景上读:先往右下描一层近黑再画正文(纯白字在亮面上会糊)。
		// 单层偏移描边足够,不用画四份。
		auto DrawCentered = [DrawCanvas, DrawFont](const FString& Text, float Y, const FColor& Shadow, const FColor& Main)
		{
			float W = 0.0f, H = 0.0f;
			DrawCanvas->StrLen(DrawFont, Text, W, H);
			const float X = (DrawCanvas->SizeX - W) * 0.5f;
			DrawCanvas->SetDrawColor(Shadow);
			DrawCanvas->DrawText(DrawFont, Text, X + 2.0f, Y + 2.0f, 1.0f, 1.0f);
			DrawCanvas->SetDrawColor(Main);
			DrawCanvas->DrawText(DrawFont, Text, X, Y, 1.0f, 1.0f);
		};
		const FColor ShadowColor(0, 0, 0, 190);

		// 线索支持多行(拾取物的 PickupMessage 是 MultiLine):逐行居中,行距 = 行高 + 8。
		TArray<FString> MessageLines;
		Ball->GetPendingMessage().ToString().ParseIntoArrayLines(MessageLines, /*bCullEmpty=*/false);
		if (MessageLines.Num() == 0)
		{
			MessageLines.Add(FString());
		}

		// 自动折行:字放大之后单行可能顶到屏幕边(线索往往很长)。按"一个中文字"的宽度
		// 估算每行能放几个字,超长行硬折 —— CJK 场景足够;拉丁文只会偏保守(可能折在词中)。
		// 作者自己敲的换行(ParseIntoArrayLines)优先保留,只在更超长时才再折。
		{
			float CharW = 0.0f, CharH = 0.0f;
			DrawCanvas->StrLen(DrawFont, TEXT("字"), CharW, CharH);
			const int32 MaxCharsPerLine = (CharW > 0.0f)
				? FMath::Max(8, FMath::FloorToInt((DrawCanvas->SizeX * 0.8f) / CharW))
				: 48;
			TArray<FString> Wrapped;
			for (const FString& SrcLine : MessageLines)
			{
				if (SrcLine.IsEmpty())
				{
					Wrapped.Add(FString());
					continue;
				}
				for (int32 Start = 0; Start < SrcLine.Len(); Start += MaxCharsPerLine)
				{
					Wrapped.Add(SrcLine.Mid(Start, MaxCharsPerLine));
				}
			}
			MessageLines = Wrapped;
		}

		float LineH = 0.0f;
		for (const FString& Line : MessageLines)
		{
			float W = 0.0f, H = 0.0f;
			DrawCanvas->StrLen(DrawFont, Line, W, H);
			LineH = FMath::Max(LineH, H);
		}
		const float LineStep = LineH + 8.0f;

		float Y = DrawCanvas->SizeY * 0.4f;
		for (const FString& Line : MessageLines)
		{
			DrawCentered(Line, Y, ShadowColor, MainColor);
			Y += LineStep;
		}

		const FKey DismissKey = Ball->GetMessageDismissKey();
		const FString KeyLabel = (DismissKey == EKeys::SpaceBar) ? TEXT("空格") : DismissKey.GetDisplayName().ToString();
		DrawCentered(FString::Printf(TEXT("按 %s 关闭"), *KeyLabel), Y + 20.0f, ShadowColor, MainColor);
		return;
	}

	TArray<FString> Lines;

	// 编译时间戳:一眼鉴定"跑的 dll 是哪次编的"——多人协作时"源码是新的但行为是旧的"
	// (dll 没重编)的仲裁证据。__DATE__/__TIME__ 是本编译单元的编译时刻。
	Lines.Add(FString::Printf(TEXT("GS build %s %s"), ANSI_TO_TCHAR(__DATE__), ANSI_TO_TCHAR(__TIME__)));

	if (bShowGravityStatus)
	{
		AGSGravityManager* Manager = Ball->GravityManager;
		const EGSGravityDirection Direction = Ball->GetCurrentGravityDirection();
		Lines.Add(FString::Printf(TEXT("Gravity: %s   (rev %d)"),
			*GSGravity::GetDirectionDisplayName(Direction),
			Manager ? Manager->GravityRevision : 0));

		if (Manager)
		{
			const TArray<EGSGravityAxis> Allowed = Manager->GetAllowedAxes();
			FString AxesText;
			for (const EGSGravityAxis Axis : Allowed)
			{
				if (!AxesText.IsEmpty())
				{
					AxesText += TEXT("  ");
				}
				AxesText += GSGravity::GetAxisDisplayName(Axis);
			}
			Lines.Add(FString::Printf(TEXT("Allowed axes: %s"), *AxesText));
		}

		Lines.Add(FString::Printf(TEXT("Camera up: (%.2f, %.2f, %.2f)"),
			Ball->GetCameraUpVector().X, Ball->GetCameraUpVector().Y, Ball->GetCameraUpVector().Z));

		// Transient "X轴不可用" hint shown after a disallowed 1/2/3 press.
		if (Ball->IsAxisHintActive())
		{
			Lines.Add(Ball->GetAxisHintText());
		}
	}

	if (bShowFallStatus && Ball->LandingResponse)
	{
		UGSLandingResponseComponent* Landing = Ball->LandingResponse;
		Lines.Add(FString::Printf(TEXT("Supported: %s   fall %.0f cm/s   dist %.0f cm   air %.2f s"),
			Landing->IsSupported() ? TEXT("yes") : TEXT("no"),
			Landing->GetCurrentFallSpeedCm(),
			Landing->GetCurrentFallDistanceCm(),
			Landing->GetAirborneSeconds()));
	}

	if (bShowCollectibleStatus && Ball->WorldStateManager)
	{
		Lines.Add(FString::Printf(TEXT("Collected: %.0f / %.0f"),
			Ball->WorldStateManager->CollectedValue,
			Ball->WorldStateManager->GetRequiredGoalValue()));
	}

	// —— 拾取/查看提示:**悬在物体正上方**(2026-09-25 用户要求"靠近后物体上方有拾取提醒的字");
	// 原先是在左下角调试块里打一行,现在改成世界点投影到画布,只在该物体进入互动半径时出现。
	if (bShowInteractionPrompt)
	{
		AActor* Target = Ball->GetCurrentInteractable();
		const FText Prompt = Ball->GetCurrentInteractionText();
		if (Target && !Prompt.IsEmpty() && Canvas->SceneView)
		{
			// 用 actor 的包围盒顶面再往上抬 40cm,提示不会压在物体身上。
			FVector BoundsOrigin, BoundsExtent;
			Target->GetActorBounds(/*bOnlyCollidingComponents=*/false, BoundsOrigin, BoundsExtent);
			const FVector LabelWorld(BoundsOrigin.X, BoundsOrigin.Y, BoundsOrigin.Z + BoundsExtent.Z + 40.0f);
			const FVector Screen = Canvas->Project(LabelWorld);
			if (Screen.Z > 0.0)
			{
				UFont* PromptFont = GEngine->GetLargeFont();
				const FString PromptStr = Prompt.ToString();
				float PW = 0.0f, PH = 0.0f;
				Canvas->StrLen(PromptFont, PromptStr, PW, PH);
				const float PX = Screen.X - PW * 0.5f;
				Canvas->SetDrawColor(FColor(0, 0, 0, 200));
				Canvas->DrawText(PromptFont, PromptStr, PX + 2.0f, Screen.Y + 2.0f, 1.0f, 1.0f);
				Canvas->SetDrawColor(TextColor.ToFColor(true));
				Canvas->DrawText(PromptFont, PromptStr, PX, Screen.Y, 1.0f, 1.0f);
			}
		}
	}

	if (bShowControls)
	{
		Lines.Add(TEXT("WASD roll  |  RMB aim / LMB flip object gravity  |  Q/E or wheel camera dist  |  O/P speed  |  F pick up / read (F again closes)  |  R reset"));
	}

	float Y = StartPosition.Y;
	Canvas->SetDrawColor(TextColor.ToFColor(true));
	for (const FString& Line : Lines)
	{
		Canvas->DrawText(GEngine->GetMediumFont(), Line, StartPosition.X, Y, 1.0f, 1.0f);
		Y += LineHeight;
	}

	// 准星(新瞄准机制):按住右键出现;锁到可改变重力的方块变绿。
	if (Ball->bAiming)
	{
		const bool bLocked = Ball->AimedBlock != nullptr;
		Canvas->SetDrawColor(bLocked ? 130 : 240, bLocked ? 255 : 240, bLocked ? 180 : 240, 220);
		const FVector2D Center(Canvas->SizeX * 0.5f, Canvas->SizeY * 0.5f);
		Canvas->K2_DrawLine(Center + FVector2D(-16.0f, 0.0f), Center + FVector2D(-5.0f, 0.0f), 2.0f);
		Canvas->K2_DrawLine(Center + FVector2D(5.0f, 0.0f), Center + FVector2D(16.0f, 0.0f), 2.0f);
		Canvas->K2_DrawLine(Center + FVector2D(0.0f, -16.0f), Center + FVector2D(0.0f, -5.0f), 2.0f);
		Canvas->K2_DrawLine(Center + FVector2D(0.0f, 5.0f), Center + FVector2D(0.0f, 16.0f), 2.0f);
	}
}
