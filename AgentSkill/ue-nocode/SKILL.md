---
name: ue-nocode
description: 通过 UE5.8 原生 MCP 无代码操控虚幻编辑器的工作流与踩坑手册。当用户要求操作 UE/虚幻引擎项目(建蓝图、摆场景、生成 Actor、材质、PIE 验证、蓝图报错排查)或遇到 MCP 连接 UE 失败时使用。
---

# UE5.8 无代码操控(MCP)工作手册

> 本文件是入口+索引;细节按症状查 `reference/` 下的主题手册。**动手前先扫一遍"红线十条"**——每条都真实踩过。

## 环境事实(2026-09)

- 引擎 `D:\UE_5.8`(中文本地化);MCP 端点 `http://127.0.0.1:8000/mcp`,协议 2025-06-18
- **编辑器没开 = MCP 连不上**;端口约定:所有项目都用 8000,**一次只开一个编辑器**,切项目=关旧开新
- 主线项目 `D:\UE\z-flip`(v5 双向 Z 重力滚球,详见其 `HANDOVER_zflip.md`);各项目现状见 `reference/PROJECT_SETUP.md`
- 编辑器日志在 `<项目>/Saved/Logs/`;编译错误、运行时错误都在这

## 三条执行通道(按优先级,细节见 reference/CONNECT_MCP.md)

1. `python ue.py py @脚本.py` —— execute_python_code(首选,需 VibeUE 加载)
2. `python ue.py call <toolset> <tool> '{json}'` —— MCP 元工具
3. `python ue_pyexec.py '<代码>'` —— 组播兜底(仅需 PythonScriptPlugin;MCP 没起时先经它发 `ModelContextProtocol.StartServer`)
脚本都在 `C:\Users\20625\.zcode\skills\ue-nocode\reference\`,直接用,别手写握手。

## 接入/操作新项目 SOP(机械执行)

0. **先确认编辑器开的是哪个项目**(get_app_state 看窗口标题)。端口在监听 ≠ 目标项目在运行
1. uproject 启用六插件:PythonScriptPlugin、EditorScriptingUtilities、ModelContextProtocol、AllToolsets、ToolsetRegistry、VibeUE
2. 两个 ini 配置 + 启动地图指向真实关卡 → 详细步骤与坑:`reference/PROJECT_SETUP.md`
3. 就绪标准:`netstat -ano | grep ":8000" | grep LISTENING`(自启不保证生效,兜底拉起法见 CONNECT_MCP.md)
4. 验证三步:HTTP 握手 → tools/list 见 execute_python_code → list_toolsets 计 82(含 VibeUE 30 服务)
5. **跑任何依赖关卡的脚本前,先显式 load_level + 断言关卡名**(当前关卡可能是未保存的 /Temp/Untitled_N)

## 目录:症状 → 手册

| 症状 / 任务 | 查 |
|---|---|
| MCP 连不上、工具没注册、选执行通道、协议协商失败 | `reference/CONNECT_MCP.md` |
| PIE 验收、按键没反应、PIE 卡住、编辑器"假死机"、模拟按键 | `reference/PIE_TESTING.md` |
| 建蓝图、连线、中文节点 id、SCS 组件、安装器脚本 | `reference/BLUEPRINT_EDITOR.md` |
| Python 属性报错、类加载 None、枚举签名、bash→python 传参 | `reference/PYTHON_API_PITFALLS.md` |
| 新项目接入、搬关卡、模板残留、场景资产行为、项目版图 | `reference/PROJECT_SETUP.md` |
| 改 GravityShift C++(重力/翻转/摄像机/刚体)、评审翻转类代码 | `reference/GRAVITY_CPP_PATTERNS.md` |
| 推/拉 GitHub、git 被墙、建仓库、令牌、同步队友改动 | `reference/GITHUB_SYNC.md` |

## 红线十条(每条都真实踩过)

1. **编辑器开着 ≠ 目标项目开着;当前关卡可能是 Untitled**——动手前先断言项目与关卡
2. **远程 python 执行期间 PIE 世界暂停**——测动态行为必须"发射→退出脚本→隔秒再读",脚本内 sleep 读到的全是冻结值
3. **PIE 撞蓝图编译错误会弹模态框,游戏线程等确认 = 假死机**——先 get_app_state 找模态框,别 taskkill
4. **睡眠刚体无视 AddForce**——重力翻转必须广播唤醒(WakeRigidBody),否则球/方块纹丝不动
5. **弹簧臂 bInheritPitch/Yaw=false 会滤掉 180° 翻转**——摄像机不跟转先查这个
6. **中文路径 C++ 编译必崩**(任何一层目录,含插件目录名)——详见 `ue-cpp-build-cnpath` SKILL;英文路径 + 引擎 dotnet 直调 UBT + `-NoUBA`
7. **git 协议被墙时直接走 api.github.com REST 直传**(脚本固化,别反复重试 git push)
8. **安装器异常时绝不创建 fallback 资产**——失败就 log+跳过,否则污染 Content
9. **execute_python_code 异常时吞 output**——重逻辑拆步或整体 try/except
10. **MSYS 编码/路径转换**:grep 模式别以 `/` 开头;bash 双引号里的 `$` 会被展开;中文别经 bash 内联传给 python/PowerShell

## 参考(脚本资产)

- `reference/ue.py` MCP 命令行助手(首选入口);`reference/mcp_http.py` 最小握手客户端;`reference/ue_pyexec.py` 组播兜底
- `reference/build_coin.py` 金币蓝图完整节点表/引脚索引/连线清单;`reference/fix_cast.py` 插入 Cast 修类型示例
- `reference/push_via_api.py` + `reference/read_gh_cred.ps1` GitHub REST 直传与令牌读取
- 相关 skill:`ue-cpp-build-cnpath`(中文路径 C++ 编译排雷);工作区 `UE无代码工作流.md`(52 工具集能力清单)
