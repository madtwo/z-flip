# MCP 连接与远程执行通道

> 什么时候读这个文件:连不上 MCP、工具没注册进会话、需要选执行通道、MCP 状态异常排查。

## 环境事实(本机)

- 引擎:`D:\UE_5.8`(中文本地化);MCP 端点:`http://127.0.0.1:8000/mcp`,协议 **2025-06-18**
- **编辑器没开 = MCP 连不上**,这是连接失败第一排查项;端口约定:所有项目都用 8000,**一次只开一个编辑器**,切项目=关旧开新
- ZCode 用户级 `~/.zcode/cli/config.json` 注册了 unreal-mcp(HTTP 型)。**协议协商失败时在客户端 UI(设置→MCP→编辑该服务器)切「兼容旧版」**;不要往 config.json 手写协议字段——该文件 schema 严格,未知键会让整个服务器条目被静默丢弃。改完重启会话生效
- 编辑器日志:`<项目>/Saved/Logs/<项目名>.log`(编译错误/运行时错误都在这)

## 就绪标准与启动

- 以 `netstat -ano | grep ":8000" | grep LISTENING` 为就绪标准;tasklist grep 在启动期间会误报"进程消失"(踩过两次)
- `bAutoStartServer=True` **不保证自启**(复制项目/新项目首次启动经常不起,原因未查清)。没起就兜底拉起:
  ```bash
  python ue_pyexec.py "import unreal; unreal.SystemLibrary.execute_console_command(None, 'ModelContextProtocol.StartServer')"
  ```
- 首次启动(含 VibeUE)耗时数分钟,耐心轮询;启动还会弹"消息日志"窗口(会抢键盘焦点,见 PIE_TESTING.md)

## 三个执行通道(按优先级)

| 通道 | 前提 | 用法 |
|---|---|---|
| `execute_python_code`(首选) | VibeUE 已加载 | `python ue.py py @脚本.py`;返回 `{success, output, result}`;支持多行 |
| MCP `call_tool` | MCP 在听 | `python ue.py call <toolset> <tool> '{json}'` |
| `ue_pyexec.py` 组播(兜底) | 只需 PythonScriptPlugin 远程执行 | `python ue_pyexec.py '<代码>'`(多行自动走临时文件) |

**封装脚本直接用,别手写握手**:
```bash
cd C:/Users/20625/.zcode/skills/ue-nocode/reference
python ue.py tools                          # 顶层工具 + schema
python ue.py toolsets                       # 工具集清单
python ue.py call ActorTools find_actors '{"name":"棚子","tag":"","collision_channels":[]}'
python ue.py py "import unreal; print(unreal.SystemLibrary.get_project_directory())"
python ue.py py @D:/tmp/脚本.py             # 多行代码走文件,中文无转义问题
UE_OUT_LIMIT=30000 python ue.py py @x.py    # 输出被截断时调大
```

## 协议/参数细节(实测)

- `call_tool` 参数名是 **`toolset_name` / `tool_name` / `arguments`**(不是 toolset/tool);`describe_toolset` 参数名 **`toolset_name`**
- 顶层工具 10 个:3 元工具(`list_toolsets`/`describe_toolset`/`call_tool`)+ `execute_python_code`、`discover_python_*`、`list_python_subsystems`、`deep_research`、`terrain_data`
- **工具集实测 82 个**(其中 30 个是 VibeUE 服务)。旧数据"258"是错的——把描述正文里 `- **Plugin**` 行也数进去了;统计工具集必须用严格正则 `^- [\w.]+:`
- **VibeUE 服务只暴露给编辑器内 Python**,原生 call_tool 会报 "Unknown tool"。必须经远程 Python 调 `unreal.BlueprintService.xxx`,方法清单 `dir(unreal.BlueprintService)` 探测
- VibeUE 启用名 = **"VibeUE"**(uplugin 无 Name 字段时插件名=文件名,不是 Fab 目录名;用目录名启用会弹 Missing Plugin)

## execute_python_code 的坑

- **脚本抛异常时 `output` 被吞掉,只回 error_message** → 重逻辑要么拆步,要么整体 try/except、关键状态 print 出来
- `ue_pyexec.py` 组播协议要点(手写客户端时):每次调用用**唯一 source id**(编辑器只维护一条连接,同源同端口二次 open_connection 不会重连);多行代码写临时文件 + `exec_mode="ExecuteFile"`;print 输出回传在 `output[]`
- **远程 python 执行期间 PIE 世界暂停**(游戏时钟冻结,物理/摄像机/Tick 全停),脚本之间才恢复 → 测动态行为必须"发射后退出脚本、隔秒再读",详见 PIE_TESTING.md 坑 2
- 组播通道偶发连不上:先重试一两次;持续失败优先排查是不是有**模态对话框**(见 PIE_TESTING.md 坑 4)

## 验证三步(接入后)

1. HTTP 握手 → 2. `tools/list`(应见 execute_python_code 等顶层工具)→ 3. `list_toolsets`(82 个工具集、30 个 VibeUE 服务)

## VibeUE 激活位置(备查)

引擎级 `D:\UE_5.8\Engine\Plugins\Marketplace\VibeUE581860d1833205V1`,2026-09-01 已激活。
