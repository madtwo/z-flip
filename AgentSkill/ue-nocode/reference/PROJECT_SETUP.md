# 新项目接入与资产搬迁手册

> 什么时候读这个文件:接入新 UE 项目、迁移关卡/资产到新项目、编辑器行为诡异(找不到资产/编译错/对象丢失)。

## 接入新项目 SOP(机械执行)

0. **先确认编辑器开的是哪个项目**:get_app_state 看窗口标题。**端口在监听 ≠ 目标项目在运行**——这是绕弯头号根源(曾按上一项目惯性操作了一整轮)。
1. uproject 启用六插件:PythonScriptPlugin、EditorScriptingUtilities、ModelContextProtocol、AllToolsets、ToolsetRegistry、VibeUE(启用名="VibeUE")
2. `Config/DefaultEngine.ini` 追加:
   ```ini
   [/Script/PythonScriptPlugin.PythonScriptPluginSettings]
   bRemoteExecution=True
   RemoteExecutionMulticastBindAddress=0.0.0.0
   RemoteExecutionMulticastGroupEndpoint=239.0.0.1:6766
   ```
3. `Config/DefaultEditorPerProjectUserSettings.ini` 追加:
   ```ini
   [ModelContextProtocol]
   ServerPortNumber=8000
   ServerUrlPath=/mcp
   bAutoStartServer=True
   ```
   (自启不保证生效,兜底见 CONNECT_MCP.md)
4. **启动地图必须指向真实存在的关卡**:新项目常写 `/Game/Maps/Main` 这种不存在的包,编辑器开在 Untitled 空关卡——"跑在 Untitled"坑的源头。改成实际关卡包路径
5. 关编辑器前先替用户保存(Ctrl+S);WM_CLOSE 后常弹"保存内容"对话框 → activate 编辑器 + 回车点默认按钮
6. 就绪标准与验证三步见 CONNECT_MCP.md

## 迁移关卡到新项目(2026-09-02 z-flip 实测 ✅)

1. **先看 umap 引用了哪些包**(决定拷什么):
   ```bash
   grep -aoE 'Game/[A-Za-z0-9_一-龥]+' "目标.umap" | sort -u   # 模式别以 / 开头(MSYS 会当路径转换)
   ```
2. **必须连带**:`Content/__ExternalActors__`、`__ExternalObjects__`(UE5 one-file-per-actor,Actor 数据在外部文件,光拷 umap = 空关卡)。注意这两个目录里只有**目标地图自己的子文件夹**有用,其他是别的地图的记录
3. 按引用清单拷 `_GENERATED`(建模网格)、`LevelPrototyping` 等用到的包;**模板套件(ThirdPerson 的 Lvl_ThirdPerson.umap + BP 壳)能不拷就不拷**
4. 拷完在编辑器里 `load_level` + 断言关卡名(见 PYTHON_API_PITFALLS.md)
5. 若 WorldSettings 藏了模板 GameMode 覆盖(PIE 报角色蓝图编译失败时查):`ws.set_editor_property('default_game_mode', None)` 后存盘

### 模板残留引用链(踩过的完整链条)

拷入 `Content/ThirdPerson`(模板图+BP 壳)→ PIE 启动拉起 `BP_ThirdPersonCharacter` 编译 → 新项目没有 `Input/`、`Characters/` 资产 → 编译失败 → **PIE 弹模态确认框,游戏线程等确认,疑似编辑器死机**(详见 PIE_TESTING.md 坑 1)。根治:不需要就整个文件夹删除(删前 grep 确认目标 umap 无引用)。

## 模板项目陷阱

- **从模板建项目必须走 Launcher**:raw 复制模板文件夹会缺 `Characters/`、`Input/`、`LevelPrototyping/`(Launcher 自动补),导致 `BP_ThirdPersonCharacter` 编译错误(EnhancedInputAction None)。修复:从正常项目拷这三个文件夹 + 重启编辑器 + 重编译
- C++ 插件/项目模块编译另看 `C:\Users\20625\.zcode\skills\ue-cpp-build-cnpath\SKILL.md`:中文项目路径必崩(cl.exe 把 UTF-8 响应文件当 GBK),UBA 偶发 Access denied → LNK1136/LNK1201,UBT 5.8 要 .NET 10 → 英文路径 + 引擎内置 dotnet 直调 UBT.dll + `-NoUBA`。**任何一层目录含中文都会进 .rsp 崩掉**(新项目一律 ASCII 路径,包括插件目录名——`重力翻转` 目录名坑过一轮)

## 场景与资产行为

- **地形高度会变**:摆放 Actor 前用 `SceneTools.trace_world`(start→end 向下)探测地面高度;天空球会挡 trace,**起点别太高**(z≤500)。曾出现出生点地面 0→290,埋掉低处 Actor
- **建模工具(CubeGrid)网格看得见走得穿**:Actor 碰撞全开但资产无碰撞体(`body_setup.agg_geom` 全 0)。修法二选一:①资产补简单碰撞或切 `CTF_USE_COMPLEX_AS_SIMPLE` 用三角面;②外面套 BlockingVolume/带碰撞盒体(测试场景最快)
- **新加的 SCS 组件不出现在已放置旧实例上**——删旧实例重摆(详见 BLUEPRINT_EDITOR.md)
- 任务进行中突然换项目/换关卡:先更新 todo 再动手,避免按旧上下文惯性操作

## 当前项目版图(2026-09-02)

- **主线:`D:\UE` 下的 z-flip**(2026-09-05 已同步至 v7:六方向重力+导轨相机+落地三带网格联动+拾取/钥匙/门,dll 已重编 PIE 实测;GitHub madtwo/z-flip public,REST 直传;仓库内 AgentSkill/gs-block-assembly/SKILL.md = 给关卡侧 AI 的积木拼装 skill;详见其 HANDOVER_zflip.md §8-§15)
- `D:\UE\MyProject2`:v2 六向已验收基线,已 pivot 停更(工作区有 v5 半成品残留,勿在其上开发)
- `D:\UE\我的项目2`:原版参考/回退,勿动
- 参考包:`D:\下载\GravityShift_ZFlip_RollingBall_DocumentPack_v5`(v5 文档规范)、`D:\下载\GravityShift_UE582_RuntimePack_v6_RC1`(GPT 写的运行时包,未编译过,仅参照)
