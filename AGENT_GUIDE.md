# AI 接手指南(给下一个 AI,含队友侧部署)

这份仓库是"AI + UE 无代码操控"工作流的完整现场:项目本体、测试关卡、AI 工作手册(skill)全都在。队友侧 AI 拿到仓库后按下面走。

## 0. 读文档顺序

1. 本文件(环境与连接)
2. `HANDOVER_zflip.md` — 完整交接史:每轮做了什么、踩了什么雷、修了什么,**别重复踩**
3. `AgentSkill/ue-nocode/SKILL.md` — 工作手册入口(按"症状→手册"目录表查 `AgentSkill/ue-nocode/reference/`)
4. 改 C++ 前必读 `AgentSkill/ue-cpp-build-cnpath/SKILL.md`(中文路径编译排雷)
5. 玩法/装配问题查 `USAGE_WHITEBOX.md` 和 `README.md`

## 1. 环境事实(本机,若换机器按此核对)

- 引擎:`D:\UE_5.8`(UE 5.8.2,中文本地化);项目:`D:\UE\z-flip`(**全 ASCII 路径,别改中文**)
- MCP 端点 `http://127.0.0.1:8000/mcp`(协议 2025-06-18);**编辑器没开 = 连不上**
- 一次只开一个编辑器(端口共用 8000);切项目=关旧开新
- 用户可能正在用电脑:不要抢前台、不要盲点鼠标;OS 级按键注入前先确认没人在操作

## 2. 连接与执行(三步)

```bash
netstat -ano | grep ":8000" | grep LISTENING        # ① 就绪标准(自启不保证生效)
# ② 没在听 → 组播兜底拉起:
python C:/Users/20625/.zcode/skills/ue-nocode/reference/ue_pyexec.py "import unreal; unreal.SystemLibrary.execute_console_command(None, 'ModelContextProtocol.StartServer')"
# ③ 之后用 ue.py(MCP)或 ue_pyexec.py(直连)执行 python:
python .../ue.py py "import unreal; print(unreal.SystemLibrary.get_project_directory())"
```
(队友机器上脚本在仓库 `AgentSkill/ue-nocode/reference/`)

## 3. 编译(改 C++ 后)

```bash
DOTNET_ROOT="D:/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64" \
"D:/UE_5.8/Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe" \
"D:/UE_5.8/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll" \
ZFlipEditor Win64 Development "-project=D:/UE/z-flip/z-flip.uproject" -WaitMutex -NoUBA
```
先关编辑器再编(dll 被锁);约 15s。

## 4. 五条铁律(全部真实踩过,细节在 skill 的 reference/ 里)

1. **远程 python 执行期间 PIE 世界暂停**——测动态行为必须"发射→退出脚本→隔秒再读",脚本内 sleep 读到的全是冻结值
2. **PIE 撞蓝图编译错误会弹模态框,像编辑器死机**——先 get_app_state 找模态框,别 taskkill
3. **睡眠刚体无视 AddForce**——翻转靠 `OnGravityChanged` 广播里 WakeRigidBody;新 Actor 的 Manager 绑定必须**懒重试**(关卡 Actor 的 BeginPlay 早于 GameMode 生成 Manager)
4. **睡眠刚体/翻转类评审清单**:力矩轴 `Up×Desired`(写反=操作镜像)、弹簧臂 `bInheritPitch` 必须真、反重力线必须大于房间内最大落体冲击
5. 中文路径/中文字面量进 bash 会被编码层搅乱——文件操作走 python,PS 脚本写 UTF-8-BOM 文件再执行

## 5. 当前状态速记(2026-09-02 晚)

- 已实测:小球 WASD 移动+制动、G 翻转(球+方块,方块贴天花板不翻滚)、落地三档(150/2000 分界,单次弹跳锁存)、摄像机 180° 跟转
- 遗留:E 开关、KillVolume/表面体积实机触发、高落差反重力展示(本房间达不到 2000 阈值)、z-flip 后续玩法迭代
- 完整验收证据与每轮修复:`HANDOVER_zflip.md` §8/§9/§10

## 6. GitHub 同步(需要推代码时)

git 协议在本机被墙(api.github.com 间歇可用)。用仓库内 `AgentSkill/ue-nocode/reference/push_via_api.py`:
```bash
GH_TOKEN=$(powershell -NoProfile -ExecutionPolicy Bypass -File ".../read_gh_cred.ps1" | tail -1) \
python push_via_api.py "madtwo/z-flip" "D:/UE/z-flip"
```
令牌读自 Windows 凭证管理器,只进环境变量别回显;仓库保持私有。详见 `AgentSkill/ue-nocode/reference/GITHUB_SYNC.md`。
