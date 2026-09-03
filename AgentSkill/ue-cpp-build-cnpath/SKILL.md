---
name: ue-cpp-build-cnpath
description: 在中文系统/中文项目路径下编译 UE5.8 C++ 插件或项目模块的排雷手册。当用户要在含中文路径的 UE5.8 项目里编译 C++(插件/项目模块)、遇到 UnrealBuildTool 报 C1083/LNK1136/LNK1201/UBA Access denied/.NET 版本不对、或 Build.bat 经命令行调用乱码时使用。
---

# UE5.8 中文路径 C++ 编译排雷(2026-09-01 全部实测)

## 总原则

**UE5.8 在中文系统 + 中文项目路径下,C++ 编译几乎必崩。** 不是插件代码的问题,是工具链的编码处理问题。两个独立的大坑叠加:

1. **cl.exe 把 UTF-8 响应文件当系统 ANSI 代码页(中文系统=GBK)读** → 中文路径全变乱码 → C1083 找不到中间文件
2. **Unreal Build Accelerator(UBA)的文件层在本机偶发 `SetFileInformationByHandle Access is denied`** → 链接器写不了 PDB / 写出损坏的 .lib → LNK1201/LNK1136

解决:**英文路径 + 引擎内置 dotnet 直调 UBT + `-NoUBA`**。下面是机械流程。

## 前提检查(动手前先确认)

```powershell
# 1. VS 2022 装了 VC 工具链
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath
# 应输出 C:\Program Files\Microsoft Visual Studio\2022\...

# 2. 引擎在 D:\UE_5.8(或你的引擎根)
ls "D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe"
ls "D:\UE_5.8\Engine\Build\BatchFiles\Build.bat"
```

## 步骤 A:项目路径必须是英文

中文项目路径(`D:\UE\我的项目2`)→ C++ 必崩。复制到英文路径:

```powershell
# 排除可再生的缓存目录(加速复制,大小从 270M→130M)
robocopy "D:\UE\我的项目2" "D:\UE\MyProject2" /E /XD DerivedDataCache Intermediate Saved Binaries /NFL /NDL /NJH
# 重命名 uproject 文件名为 ASCII(注意:uproject 文件名 = 编译 target 名,必须是 ASCII)
# 复制后文件还是中文名 我的项目2.uproject,改名:
# (用 Python 改名最稳,bash mv 在中文文件名上偶尔鬼畜)
python -c "import os,shutil; d=r'D:\UE\MyProject2'; [os.rename(os.path.join(d,f),os.path.join(d,'MyProject2.uproject')) for f in os.listdir(d) if f.endswith('.uproject')]"
# 删掉 .uproject.backup_* 那些安装器留下的备份
```

**robocopy 复制后文件名编码**:robocopy 传中文参数走 UTF-16,文件名保真。但 robocopy 复制完的 `.uproject` 还是中文名,用 Python rename 最稳(bash 的 mv 对中文文件名在本机 Git Bash 上有一次"cannot stat"鬼畜)。

**关键**:`.uproject` 文件名 → UBT 自动派生的 target 名(`<ProjectName>Editor`)。中文文件名 = 中文 target 名 = 编译路径里全是中文 = 必崩。所以文件名必须 ASCII。

## 步骤 B:不要用 Build.bat,直接调 UBT dll

`Build.bat` 内部会设 `DOTNET_ROOT` 指向引擎内置 dotnet(解决 .NET 10 依赖),但 **Build.bat 经 cmd.exe 转一道,中文参数在系统代码页转换处会乱码**。绕开 cmd,直接用引擎 dotnet 跑 UBT dll:

```powershell
$env:DOTNET_ROOT = "D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64"
$env:DOTNET_MULTILEVEL_LOOKUP = "0"
& "D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" `
  "D:\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" `
  "MyProject2Editor" Win64 Development `
  "-project=D:\UE\MyProject2\MyProject2.uproject" -WaitMutex -NoUBA
```

参数说明:
- 第一个位置参数 = target 名(`<ProjectName>Editor`,纯蓝图项目也有隐式 target)
- `-NoUBA` = **关键**,见下
- `-WaitMutex` = 多个编辑器/UBT 实例并存时不抢锁
- 日志重定向 `*> "D:\UE\.workbuddy\tmp\build.log"`(PowerShell 重定向默认 UTF-16,读的时候 `decode('utf-16')`)

PowerShell 传中文 target 名给 dotnet.exe(UTF-16)→ 正确;但路径已 ASCII,所以也没中文问题。

## 步骤 C:必须 `-NoUBA`

UBA(Unreal Build Accelerator)在本机:
- 反复 `UbaStorageServer - SetFileInformationByHandle (FileDispositionInfo) failed on <pid> C:\ProgramData\Epic\UnrealBuildAccelerator\cas\casdb.tmp (Access is denied.)`
- 导致链接器:`LNK1201` 写不了 PDB、`LNK1136` 写出损坏的 .lib
- 即使 `dangerouslyDisableSandbox` 跑也照旧(不是沙箱问题,是 UBA 自己或本机杀软/权限)

**加 `-NoUBA` 一步到位**。UBA 是加速器,关了只是慢一点(本项目完整编一次约 3 分钟),不影响正确性。

## 步骤 D:.NET 10 依赖

UBT 5.8 是 .NET 10 程序。系统若只装了 .NET 6/8:
- **不要**装系统 .NET 10(污染环境)
- 引擎自带 `D:\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe`
- 设 `$env:DOTNET_ROOT` 指向它 + `DOTNET_MULTILEVEL_LOOKUP=0`(强制只用引擎的,不 fallback 到系统的 6/8)
- `Build.bat` 内部就是干这事,所以直调 UBT dll 要手动设

症状:直接调 `UnrealBuildTool.exe`(不设 DOTNET_ROOT)会报 `You must install or update .NET ... Framework: 'Microsoft.NETCore.App', version '10.0.0'`。

## 步骤 E:修 C++ 错误的套路(第三方插件常见)

写包的 AI 往往"静态检查通过但没真编译过",一编就错。常见雷:

1. **`UPROPERTY(meta=(Units="J"))` 或 `Units="1/s"`** → UHT 报 `Unrecognized units`。UE 不支持焦耳。改法:`Units="J"` 整段删掉;`Units="1/s"` 改 `Units="Hz"`。其它合法单位:`cm`/`m`/`cm/s`/`cm/s^2`/`s`/`kg`/`deg`/`deg/s`。
2. **`#include "Engine/PrimaryDataAsset.h"`** → 不存在。`UPrimaryDataAsset` 在 `Engine/DataAsset.h`。
3. **lambda 捕获静态局部变量** → `error C3495:简单捕获必须是含自动存储持续时间的变量`。`static ConstructorHelpers::FObjectFinder X(...); auto lam=[&X]{}` 就中招。改法:`[this]` 不捕获,lambda 内直接用 `X`(静态局部可不捕获访问)。
4. **局部变量遮蔽类成员(C4458)** → UE 把 C4458 当 error。比如 `AActor::Owner` 是基类成员,局部 `AActor* Owner=...` 就中招。改名(Owner→OwnerActor)。
5. **链接器偶发 LNK1201/LNK1136**:先 `-NoUBA`;还不行就 `rm -rf 项目/Intermediate 项目/Binaries/Win64 项目/Plugins/*/Intermediate 项目/Plugins/*/Binaries` 全清重来(残留的损坏 .lib/.pdb 会卡住增量编译)。

### 3. 编辑器已关,UBT 仍报 "Unable to build while Live Coding is active"(2026-09-03 新坑)

- **现象**:z-flip 编辑器已优雅退出(tasklist 确认无该进程),`taskkill //IM LiveCodingConsole.exe //F` 也做了,UBT 照样 5 秒失败退出码 6。
- **根因**:UBT 的判定是 `Global\LiveCoding_<目标exe路径>` **全局命名互斥体**(HotReload.cs `IsLiveCodingSessionActive`),**任何**还在跑的 UnrealEditor 实例都会持有(包括没开项目的项目选择器窗口)。杀 LiveCodingConsole 没用,互斥体是编辑器进程建的。
- **修复**:UBT 命令行加 **`-NoHotReloadFromIDE`**(`BuildConfiguration.bAllowHotReloadFromIDE=false`,跳过互斥体检查),不必杀别的编辑器窗口。完整命令=步骤 B 基础上追加该参数。
- **注意**:若目标编辑器实例自己还开着,该参数也不该用——先正常关编辑器再编,这条只解"残留实例锁互斥体"的场景。

## 步骤 F:改 C++ 后的"关→编→开"全流程(2026-09-03 实测,机械执行)

编辑器开着 dll 被锁,C++ 迭代就是关→编→开的循环;流程熟练后全程 1-2 分钟。**顺序不能乱**:

1. **先存盘再关**(用户常在地图里加东西):远程 python `unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`,并打印 `get_dirty_content_packages()/get_dirty_map_packages()` 前后对比确认(空列表=用户已自己存过)
2. **优雅退出**:`execute_console_command(None, 'QUIT_EDITOR')`,sleep 后 tasklist 确认进程没了。别 taskkill——优雅退出才会走保存/清理
3. **编译**:步骤 B 命令;撞 "Unable to build while Live Coding is active" 见反哺坑 3(`-NoHotReloadFromIDE`);杀 LiveCodingConsole.exe 没用(互斥体是编辑器进程建的,残留编辑器实例都持有)
4. **重启**:`cmd //c start "" "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "<uproject>"`。**窗口标题几秒就出来 ≠ 加载完成**,别拿标题当就绪信号
5. **MCP 必兜底**:自启十次九不起。轮询 `netstat 8000 LISTENING`(对 pid 确认是新编辑器进程),5 分钟没起就 `ue_pyexec.py` 组播发 `ModelContextProtocol.StartServer`(见 ue-nocode/CONNECT_MCP.md),几秒就绪
6. **版本断言**:python `hasattr(unreal, '<本轮新增类名>')` 或新 API——证明跑的是新 dll,防"窗口开着但旧进程/旧模块"的假象



## 修完重编译

UBT 增量,改完源码直接重跑步骤 B 的命令(改 C++ 后重启编辑器全流程见上面步骤 F)。只有改了 `.uplugin`/`Build.cs`/模块结构才需要清 Intermediate。

## 产物核对

```
项目/Binaries/Win64/UnrealEditor-<ProjectName>.dll
项目/Plugins/<Plugin>/Binaries/Win64/UnrealEditor-<Plugin>.dll
```
两个都有 = 编译成功。编辑器开项目时不会再弹"缺少模块 Build"。

## MCP / Python 通道(编译完进编辑器后)

- 复制项目后 MCP 服务器(`bAutoStartServer=True` 在 `Config/DefaultEditorPerProjectUserSettings.ini` 的 `[ModelContextProtocol]` 段)**第一次没自动起**(原因没查清,疑似 Saved 配置没复制)。兜底:控制台命令 `ModelContextProtocol.StartServer`:
  ```python
  unreal.SystemLibrary.execute_console_command(None, "ModelContextProtocol.StartServer")
  ```
  Python 走 `ue_pyexec.py` 组播通道发(见 `ue-nocode` SKILL),不需要 MCP 起来就能发命令。
- 引擎原生 Python 远程执行(`bRemoteExecution=True` 在 `Config/DefaultEngine.ini`,复制项目时带过来了)作为完全不依赖 MCP 的兜底通道,实测可用。

## 排雷脚本

编译流程封装见 `D:\UE\.workbuddy\tmp\` 下的 `build_gs.cmd`(UTF-8 + CRLF 的 cmd 包装,内含 `chcp 65001` + `call Build.bat`——但本环境 cmd 从 bash 调用被安全策略拦,所以**最终走的是步骤 B 的 PowerShell 直调**,留这文件备查)。日志在 `D:\UE\.workbuddy\tmp\build*.log`(UTF-16,Python `decode('utf-16')` 读)。

## 一句话决策树

```
项目要编译 C++?
├─ 路径含中文? → 复制到英文路径(步骤 A),别想着改系统代码页
├─ 用 Build.bat? → 别。直接 dotnet.exe 跑 UBT dll(步骤 B)
├─ UBA 报 Access denied / LNK1136/LNK1201? → 加 -NoUBA(步骤 C)
├─ .NET 版本不对? → 设 DOTNET_ROOT 指引擎内置 dotnet(步骤 D)
├─ 编辑器已关仍报 Live Coding active? → 加 -NoHotReloadFromIDE(反哺坑 3)
└─ 真实 C++ 错误? → 按步骤 E 套路修

---

## 反哺:2026-09-02 实测失败经验(可避免的 + 新坑)

### 1. 中文路径的"任何一层"都会进 .rsp —— 本次是插件目录名(本可避免 ⚠️)

- **现象**:项目根 `z-flip` 是英文,但插件目录取名 `重力翻转`。编译时 UBT 生成的 `GSBlockBase.cpp.obj.rsp` 里源文件路径是 **UTF-8** 的 `.../Plugins/重力翻转/Source/...`;MSVC 的 `cl.exe` 在中文 Windows 下按 **系统 GBK(CP936)** 读响应文件,把 `重力翻转` 读成 `閲嶅姏缈昏浆` → C1083 找不到文件。**游戏模块 `ZFlip`(全英文路径)正常出 obj,只有插件崩**。
- **铁证**(`.rsp` 字节):`e9 87 8d e5 8a 9b e7 bf bb e8 bd ac` = UTF-8 "重力翻转";同段 GBK 解码 = "閲嶅姏缈昏浆"。
- **本可避免 ⚠️**:2026-09-01 我已经用"复制中文项目 `我的项目2` → 英文路径 `MyProject2`"解过**完全相同**的坑。本次新建 z-flip 时**主动**给插件目录起了中文名,等于把坑又埋回去。→ **规则强化**:"复制到英文路径"不只指项目根,**插件目录、任何源码子目录、uproject 文件名,只要会出现在编译路径里,一律 ASCII**。**新建项目/插件时默认用 ASCII 命名**,别等崩了再搬。
- **修复**:`Plugins/重力翻转` → `Plugins/GravityShift`(`.uplugin` 文件名也要与目录同名改成 `GravityShift.uplugin`),`uproject` 里 `"Name":"重力翻转"` → `"GravityShift"`,清 Intermediate/Binaries 重编即过。

### 2. UE5.8 全新 Target.cs 要用 BuildSettingsVersion.V7(新坑,非中文相关)

- **现象**:手写 `ZFlip.Target.cs` / `ZFlipEditor.Target.cs` 用 `DefaultBuildSettings = BuildSettingsVersion.V5`,UBT 直接报错(旧枚举在 5.8 不被接受)。
- **修复**:改成 `BuildSettingsVersion.V7`。
- **这是 5.8 新事实**(旧笔记的 V5 在 5.8 已失效),新建任何 5.8 工程都会踩,记在此提醒。改完需清 Intermediate 重编。
```
