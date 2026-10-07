# PIE 验收与测试方法论

> 什么时候读这个文件:要做 PIE 验收、"按了键没反应"、PIE 卡住不动、编辑器疑似死机、需要模拟真实按键。

## 验收清单(每个交付必过)

1. `compile_blueprint` 返回 null(返回文本 = 错误详情)
2. `save_assets` 返回 true
3. StartPIE → 日志无 `LogScript`/Blueprint Runtime 错误 → StopPIE
4. **让用户实际游玩**(有些效果——尤其需要输入的——截图/脚本验证不了)

## 坑 1:PIE 启动撞"蓝图编译错误"弹模态确认框 = 假死机 ⚠️⚠️

- **现象**:PIE 请求发出后日志停在 `蓝图编译失败: BP_xxx`,游戏线程完全冻结:远程执行超时、MCP 无响应、8000 还在听 → 极像编辑器死机(踩过:误杀两个编辑器实例)。真相是 UE 弹了**模态框**「确定要在编辑器中播放吗?以下蓝图资产存在未解决的编译器错误 [编辑器中运行] [取消]」,游戏线程停在等确认。
- **排查顺序**:日志尾部时间戳还在走吗 → CUA `get_app_state` 找模态框 → 才考虑进程问题。**别急着 taskkill**。
- **点掉它的正确姿势**(UE 对话框是 Slate 自绘,**无 Win32 Button 子控件,BM_CLICK 无效**):
  1. CUA 截图拿按钮在**窗口光栅**里的像素坐标 (bx,by);
  2. `GetWindowRect` 拿物理矩形,换算 **物理 = 光栅坐标 × (rect宽/光栅宽)**(本机 150% 缩放,CUA 坐标是逻辑点);
  3. `PostMessage(hwnd, WM_LBUTTONDOWN/UP, wParam, lParam=y<<16|x)` client 坐标直接点——不动真实光标、不抢前台(用户在用电脑时唯一安全路径)。
  - 旁坑:`EnumWindows` 回调里的 `Write-Output` 会丢输出(标题打印全空,"零窗口"结论不可信);中文窗口标题的 PS 脚本用 python 写 **UTF-8-BOM** 文件再执行,别内联。
- **根治**:PIE 前铲掉编译错误源头。新项目拷来的模板蓝图(如 ThirdPerson)会拉起缺资产角色的编译链——WorldSettings 里还可能藏着 `DefaultGameMode=BP_ThirdPersonGameMode` 覆盖,python `ws.set_editor_property('default_game_mode', None)` 后存盘。整个模板文件夹不用就直接删。

## 坑 2:远程 python 执行期间 PIE 世界暂停 ⚠️⚠️

- **现象**:脚本里翻转重力后 `time.sleep(4)` 再读:球不动、摄像机不转、游戏时钟 delta=0.00 —— 看起来像物理/摄像机/刚体全坏了(差点据此改错 C++)。
- **真相**:`ue_pyexec.py`/MCP 的 python **执行期间 PIE 世界暂停**(时钟冻结),脚本退出后恢复;脚本之间的真实时间里世界正常 tick。
- **正确测法**:**发射后立即退出脚本**(fire)→ bash `sleep N`(世界自由跑)→ 新脚本读结果(read)。所有动态观测都用这个三段式。
- 旁支:编辑器窗口在后台时 UE **深度节流**(真实 1s ≈ 零点几秒游戏时间),取样留足裕量;让用户游玩时把编辑器切前台。
- **亚秒级过渡的粗采样:时间膨胀放慢游戏时间**——`unreal.GameplayStatics.set_global_time_dilation(w, 0.05)`(物理/平滑在游戏时间里行为不变)。ue.py 每次调用固定开销 ~1.7s 实时,不膨胀只能采到 ~1.7s 游戏时间的粒度;膨胀 20× 后一次调用只推进 ~0.085s 游戏时间,1.2s 的相机过渡可采到 15+ 个点(2026-09-09 G 翻转过渡实测:偏移圆弧连续、总时长与理论吻合)。测完务必恢复 1.0。
- **暂停/恢复会给下一帧塞异常 dt(可为负)**:脚本执行→恢复的瞬间,`DeltaSeconds` 可能异常甚至为负——指数平滑代码 α<0 会**外推到目标反方向**(2026-09-03 轨相机实测:锁轴相机一度偏出 28.4cm 后又自愈)。C++ 里凡做平滑/积分的 Tick 代码一律 `DeltaSeconds = FMath::Clamp(DeltaSeconds, 0.f, 0.1f)`;远程验证平滑类行为读到"离谱后又恢复"的读数,先怀疑这个,别急着改数学。

## 坑 3:弹出窗口偷键盘焦点

- 关卡加载会弹"消息日志"窗口,焦点被抢,后续按键全进日志窗口(对用户同理:先点一下游戏画面再按键)。
- CUA 对该窗口的坐标点击会被帧校验拒;**PostMessage WM_CLOSE 关窗口句柄最稳**(PIE 前关掉)。

## "按了键没反应"排查链(按顺序,别跳步)

1. **先读源码确认绑定代码存在且默认开**(如 `bEnableDebugKeyInput`/`DebugNextGravityKey`);实例上再读一遍属性值,FKey 用 `key.get_editor_property('key_name')` 看真实键名(直接打印是空 `{}`,别被骗)。
2. **绑定代码执行过没有**——BeginPlay 里的相邻日志是免费证据。
3. **键事件到达 PlayerInput 没有**:PIE 里 `pc.is_input_key_down(键结构体)`(别用 `unreal.Keys`,本环境没有;拿 actor 上存的 FKey 传进去)。
4. `ke <键> Down` 控制台命令**会触发 InputComponent 的 BindKey 处理器,但不更新 is_input_key_down 状态表**——两个通道分开看,别拿一个否定另一个。
5. **OS 级真实按键注入**(最像用户操作的验收):PowerShell 先 `SetProcessDPIAware()`(否则坐标被缩放,键打到别的位置);CUA `cursor_position` 回读校验落点 → 点击聚焦游戏视口 → `keybd_event`(0x47=G, 0x52=R, 0x57=W),KEYUP 标志=2。**用户本人正在用电脑时别注入,改让其自测**。
6. 还不行 → 下一节的输入栈时序坑。

## BeginPlay EnableInput 时序坑(根因案例)与标准修复模式

关卡摆放的 Actor 在 BeginPlay 里 `EnableInput(PC)+BindKey`:绑定和键都正常,**但推到 PC 输入栈的组件在关卡实例上会丢失**(同 BeginPlay 里子 Actor 组件生成的原生实例没事)。实测:关卡实例 revision 恒 0,手动 `enable_input(pc)` 一次立刻恢复。

**修复模式:不依赖输入栈投递——Tick 轮询 + 边缘检测**:
```cpp
PrimaryActorTick.bCanEverTick = true;  // 注意关卡里保存的旧实例 TickSettings 可能是旧值
// Tick():
const bool bNextDown = PC->IsInputKeyDown(DebugNextGravityKey);
if (bNextDown && !bWasNextKeyDown) HandleDebugNextGravity();  // 边缘检测天然免疫长按重复
```
轮询读 PlayerInput 原始键状态,只要键进了游戏视口就一定可见,与输入栈时序无关。v2 六向(2026-09-01)与 v5 滚球(2026-09-02)均用此模式。

## PIE 内 Python 验收法

```python
w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()   # PIE 世界
actors = unreal.GameplayStatics.get_all_actors_of_class(w, 蓝图生成类)
pawn  = unreal.GameplayStatics.get_player_pawn(w, 0)
```
- 读两次 Yaw 验证旋转;`pawn.set_actor_location(loc, False, False)` 传送玩家观察吸附/触发/销毁
- 读状态用 getter 方法优先(`get_gravity_direction()`),属性名靠 TypeError 提示逐个补参
- 注意 PIE 世界是 `/Game/UEDPIE_0_<关卡名>`,编辑器世界与 PIE 世界是两份;编辑器关卡摆位不随 PIE 改变

## 给用户搭测试现场:标准装配线(2026-09-13 第二关测试轮定型)

> 场景:用户说"打开 XX 关卡/第 N 关,我要测试"——AI 把现场摆好,用户只负责玩。

1. 起编辑器(没开时)→ 轮询 `netstat :8000 LISTENING`;没自启就组播发 `ModelContextProtocol.StartServer`(实测 5s 就绪,见 CONNECT_MCP.md)。
2. `load_level(目标图)` + 断言;PIE 活跃时绝不切图(坑 6)。
3. 视口相机摆到测试区域:`LevelEditorSubsystem.set_level_viewport_camera_info(loc, rot, '')`——5.8 第三参必填(见 PYTHON_API_PITFALLS)。
4. `editor_request_begin_play()` → 轮询 `is_in_play_in_editor()`。
5. **把玩家送进测试区域**(UE5.8 没有 Play-From-Here API):正常起 PIE 后传送——
   ```python
   w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
   ball = unreal.GameplayStatics.get_player_pawn(w, 0)
   ball.set_actor_location(unreal.Vector(x, y, z), False, False)
   for c in ball.get_components_by_class(unreal.PrimitiveComponent):
       c.set_physics_linear_velocity(unreal.Vector(0, 0, 0))
       c.set_physics_angular_velocity(unreal.Vector(0, 0, 0))
   ```
   隔几秒读回**位置+速度双确认**落稳(本轮 (800,-400,200) → 停 z=-50、速度 0 = 地面 z≈-100 + 球半径 50;只读位置会被"悬空/缓慢下滑"骗过)。
6. 收尾三件:①WM_CLOSE 关"消息日志"(坑 3);②`SetForegroundWindow` 主编辑器窗口(标题含"虚幻编辑器");③提醒用户**先点一下视口**再操作。
7. 截屏核对 dll:截屏前先把编辑器切前台(CUA 抓的是桌面最上层,不切会抓到用户正在看的别的窗口);PIE 画面 HUD 顶行 `GS build <日期> <时间>` = "跑的是哪版 dll"的铁证(本轮截到 08:02:20,与 dll mtime 吻合)。

**找落点先探地板**:多层白盒从高空向下打线,命中的是顶层地面/屋顶;找"内部楼层"要把探针起点放到顶层之下再打,或拿已知落点旁证(本轮塔分层:地面 z≈-100、中 500-700、顶 1000,即竖井旁逐点扫出)。

## 历史案例速查

- **v2 六向 G/R 修复**(2026-09-01):BindKey 对关卡实例失效 → 改 Tick 轮询,重编译 14s,OS 注入验收 G=横移 2156cm、R=RESET 归位 ✅
- **v5 滚球翻转验收**(2026-09-02):toggle ACCEPTED→球升空(广播唤醒)→摄像机 slerp→撞棚顶 FALL_THRESHOLD 自动反向→reset 语义 NO_CHANGE 正确 ✅。遗留:用户实机 WASD/G/E/R 游玩验收
- **金币案例**(PIE 实测 ✅):传送玩家观察吸附/销毁的验收法即出自此,详见 BLUEPRINT_EDITOR.md

## 坑 5:编辑器 autosave 把测试场景写进用户关卡 umap(三度复发:§13.6/§14.6/§15.2)

- 编辑器层摆测试 actor + PIE 时,autosave 会把脏关卡**写穿到真 umap 文件**(哪怕事后 destroy actor,磁盘 diff 已发生)。
- **标准处置**:PIE 测试跑完第一件事 `git status` 查 umap → 脏了就 `git checkout -- <umap>` 回退;测试 actor 在编辑器世界 destroy 后重载 `EditorLoadingAndSavingUtils.load_map` 清脏标记更稳。
- `EditorLoadingAndSavingUtils` 没有 `set_dirty_package`;治本:能不开编辑器世界摆场的验收,改在 PIE 世界里临时 spawn。
- **污染不止 umap,CDO 改动会脏 .uasset**:远程 python 里 `get_default_object(类).set_editor_property(...)` 设调试标记 → BP 包变脏 → autosave 把调试默认值写进 .uasset → 它会混进下一次 commit。commit 前 `git status --short` 审查,多出来的 .uasset 照样 checkout 恢复(2026-09-08 BP_GSRollingBallPawn 实例,amend 摘掉)

## 逐帧行为数据的正确采集法:C++ 内日志,不是 python 采样(2026-09-08 相机抖动轮定案)

- python 组播执行期间 PIE 世界暂停,脚本之间才恢复——python 只能采到"粗粒度时间点",**永远采不到逐帧**。要逐帧数据(抖动/平滑/泄漏量),在 C++ 里加 UPROPERTY 门控(默认关)的逐帧 `UE_LOG`,PIE 里把开关设在**PIE 实例**上(别走 CDO,见 PYTHON_API_PITFALLS),跑完解析 `Saved/Logs/<项目>.log`
- 打点技巧:**读"写入前"的状态**(如相机枢轴上帧写入值被本帧漂移到哪),它减上帧写入值=每帧泄漏量,直接定位"谁绕过了平滑层"
- 解析用 python 正则逐行抽数、算 mean/max——日志行即帧,统计即证据(本次:39 帧漂移全 0,一眼定案)
- **checkout 撞 "unable to unlink ... Invalid argument"**:编辑器正加载着该 umap,句柄占用。三步回退:`load_map('/Engine/Maps/Templates/Template_Default')` 切走 → `git checkout -- <umap>` → `load_map('/Game/原关卡')` 切回(2026-09-05 实测)
- **复现"需要持续输入"的场景(爬楼梯/长距离滚动):别用 OS 按键注入**,加一个 UPROPERTY 门控的"自动前推"调试开关(每 tick 覆盖 MoveInput 为满前推,如 `bDebugAutoDriveForward`),脚本把球放到起点即可自动跑完全程——与逐帧日志配套(2026-09-09 爬楼梯相机抖动轮实测)。注意 `set_move_input` 只生效一帧(PollNativeInput 每 tick 用键盘状态覆盖),持续驱动必须走 C++ 开关。
- **日志行=帧,别把采样间隔当帧间隔**:编辑器在后台深度节流时一帧真实 dt≈0.3s、游戏 dt 被钳到 0.1,日志仍是逐帧;解析出的"相邻行"就是相邻帧,方波状跳变=逐帧翻转,不是采样混叠。

## 交接手册可行性验收法:模拟对方处境(2026-09-05,手册"积木拼装"就靠这个落地)

- 写"照做就能行"的文档,验收标准不是"文档写完了",而是**自己当一次fresh 用户**:新建空关卡 `unreal.EditorLevelLibrary.new_level('/Game/Maps/临时名')`(返回 False 但实际建好切过去了,别被吓到),只按手册摆件 → PIE → 逐条断言 → 清理(删临时 umap + 切回原图 + checkout 被碰脏的 umap)
- 新关卡上断言"零配置可玩":玩家 pawn 类名(GSRollingBallPawn=GameMode 全局生效)、Manager 数=1、重力方向、球落稳 z
- 编辑器世界摆 StaticMeshActor:`EditorActorSubsystem` **只有 `spawn_actor_from_class`**(没有 spawn_actor);网格用 `static_mesh_component.set_static_mesh(load_asset(...))`
- 这套跑一遍,写进手册的每一步都有实测背书,对方 AI 照抄不会掉坑


## 坑 6:PIE 活跃时调 load_map = 游戏线程死锁(2026-09-08 实炸一次,代价=杀进程重启)

- **场景**:用户正在 PIE 里玩,AI 的脚本调 `unreal.EditorLoadingAndSavingUtils.load_map(...)` 切图 → 游戏线程卡死,之后**所有远程通道(MCP 组播)全部超时**,窗口因线程阻塞连激活/点击都被拒 → 只能杀进程重启。
- **铁律:任何 load_map/切关卡之前,先 `unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor()`,True 就绝不切图**(要么等用户退出 PIE,要么先 `editor_request_end_play()` 再切)。
- **铁律加强(2026-09-08 第二次踩,又杀了一次进程):`end_play()` 和 `load_map()` 绝不能写在同一个脚本里**——end_play 是异步请求,同一脚本里紧跟的 load_map 仍会撞进未结束的 PIE 直接死锁。正确姿势:end_play 单独一个脚本 → **下一个脚本先验证 `is_in_play_in_editor()==False`** → 再 load_map。load_map 之前先扫日志尾部有没有 PIE 活动(用户可能正在玩)。
- 临时校准/测试台:一次性关卡(new_level)最干净,用完删;**校准地板要厚**(≤50cm 薄板会被高速球穿透,球一穿数据全污染——用 ≥500cm 厚板或保证顶面平稳)。
- 判断"卡死还是慢":日志尾部还在出帧 → 慢;停在某行不动 + 双通道超时 + 窗口激活失败 → 死锁,别反复重试,先看有没有未保存内容(编辑器右下角"所有已保存"),能杀就杀。
- 相关:用户可能**正在玩**(日志里会有 `[GravityShift] dir=...` 等游玩痕迹),动手切图/摆件前先扫一眼日志尾部有没有 PIE 活动。
- **脚本"注入"的值也活不过恢复帧**:远程执行恢复的那一帧 dt 异常大,`ApplyMovement` 的松键刹车 `Planar *= exp(-3·dt)` 会把刚设的速度一次抹平(实测 `set_ball_linear_velocity(900)` → 第一个 tick 后只剩 15cm/s,球几乎没动,白排查一轮)。产线代码的 dt 钳位只保护平滑/积分,**减速/阻尼类公式同样要钳**;测运动别注入速度,用**游戏内驱动**(`bDebugAutoDriveForward` 调试开关;或 `enable_native_polling_input=False` + `set_move_input`,后者适合"墙上 A/D 爬升"这类自动驱动给不了的方向)——它们每 tick 从游戏侧施力,不受暂停帧影响。

## 测"运动状态"的两个隐蔽坑(2026-09-11 转向器双向轮实踩,接着上一条)

- **游戏内驱动会把注入速度"改向"**:用 `bDebugAutoDriveForward` 抵消松键刹车后,若驱动的方向与注入速度不同向,恢复帧的巨大 dt 会把速度直接盖成驱动方向——现象是"明明球就在触发盒里、速度也给了,就是不触发"(进入判定按方向判定)。对策:测"靠注入速度进入"的用例时把 `DriveAccelerationCm` 调低(100),让驱动只当"防刹车"用;测"靠驱动进入"的用例才需要大驱动力。
- **相机航向是 Pawn 实例的残留状态**:自动驱动的方向 = 相机前向在支撑面上的投影,而航向在 PIE 会话里跨多次脚本累加(每次 `add_camera_look_input` 都是相对量)→ 同一会话里连续测多个方向会跑偏。对策:按**绝对角度**重设——读 `pawn.get_editor_property('camera_pivot').get_forward_vector()` 的水平分量反推当前航向角,再 `add_camera_look_input(目标角−当前角)`;或者每个方向用例重启一次 PIE。
- **顺带:CDO 写入被引擎安全层拦**(`Blocked unsafe Python code: get_default_object() modification`)——调试开关只能在**实例**上 `set_editor_property`;这也解释了更早"改了 CDO、PIE 新实例读不到"的谜团(写入根本没生效)。

## PIE 真实输入注入:四条通道与选择(2026-09-14 两轮专家包验收定型)

> 本机注入的完整能力矩阵与"焦点前提"配方见 `WINDOWS_INPUT_LIMITS.md`;本节只列验收常用结论。

要验"真实按键/鼠标路径"(E 拾取、RMB 使用、WASD 驾驶等),四条通道实测结论:

| 通道 | 结论 |
|---|---|
| `ke <键> Down` 控制台命令 | **对 EnhancedInput 项目完全无效**(W 按下球不动)。只走 Legacy BindKey 路径,别用 |
| `PostMessage(WM_KEYDOWN)` | **被引擎丢弃**(不报错但输入不进)。鼠标的 PostMessage 点击对 Slate 有效,键盘无效 |
| **CUA 工具(首选)** | `open_application(activate=true)` 激活编辑器 + `left_click` 点一下视口拿键盘焦点,之后 `key`/`hold_key`/`click`/`right_click` 全部有效。`hold_key` 是唯一"按住 N 秒"通道,但调用阻塞、无法在按住期间并行采样 |
| **OS 级 keybd_event/mouse_event(按住+采样最佳)** | 配合 CUA 激活后,在**同一个 bash 脚本**里按序执行 `key down → sleep N → ue_pyexec 读 → rg_shot 截图 → key up`。分开调用会因思考延迟错过时间窗口(踩过 4 次) |

**前提与坑(每条都实踩过)**:
- 注入前确认前台窗口是编辑器(`GetForegroundWindow`);**键会打到当前前台窗口**——实测打到了用户正在看的浏览器上。
- `SetForegroundWindow` 从后台脚本调用会被 Windows 前台锁定拒绝(不报错但不生效)→ 用 CUA 的 `open_application(activate=true)` 代替。
- **重启 PIE 后视口焦点丢失**,必须重新 `left_click` 视口,否则后续注入全部无效。
- CUA **没有"右键按住"**(只有 right_click 单击);`SendInput`/`mouse_event` 的 `MOUSEEVENTF_ABSOLUTE` 坐标要归一化到 0..65535(传像素值光标会飞到屏幕角)。
- 单击(down/up 间隔 <1 帧)只能触发"按下一帧内完成"的逻辑(耕地 ✓、kick 级别冲量 ✗),**测不了按住持续效果**——要靠通道 4。
- 每个 `ue_pyexec` 执行期间世界暂停,读到的是**暂停瞬间**的状态;"按住期间采样"不受影响(按键状态在恢复后保持)。
- Git Bash 里写 python 脚本:**脚本内部硬编码的 `/tmp/x` 不会被路径转换**(只有命令行参数里的会被 MSYS 转),脚本里用 `C:/Users/<用户>/AppData/Local/Temp/` 全路径。

### 编排:注入器与 runner 的进程生命周期(2026-09-19 实测,真实 RMB 验收轮)

真实输入验收 = **probe(UE 内 Python)** + **injector(OS 级键鼠)** + **runner(PowerShell)** 三方协作。时序错一次,结论就是假的。

- **握手用 flag 文件,不用 sleep**:probe 进入等待态后写 `*_ready.flag`,injector 只在校到 flag 后才动作,动作完成写 `*_down/up.flag`,probe 见到才进下一段。实测时序漂移可达 20 秒,固定 sleep 必错。
- **★ runner 必须 `Start-Process -PassThru` 拿到两个 Process 对象,并等"真实 UE 进程"退出**:直接 `& $Editor …`(GUI 程序)在 Windows 上可能**立刻交回控制权**,旧 runner 于是走"固定 10 秒"清理 → **injector 在 flag 出现之前就被杀了**,症状是"注入器什么都没做、没有任何日志"。正确做法:UE 不退出就绝不杀 injector,另设一个**总墙钟超时**(实测 180s)兜真卡死。
- **injector 的 stdout/stderr 必须自己落盘**:它是唯一能证明"我等到 flag 了 / 我按下去了"的证据。
- **清理动作与判定动作分开,且清理不许抛**:实测 runner 在清理分支里对一个已退出进程取 `ExitCode` 抛异常,报告于是写成"injector failed",**盖住了 probe 真正的 `REAL_INPUT_DOWN_TIMEOUT`** —— 整轮被误读成另一类故障。
- **注入前先清残留按键状态**(上一轮卡住的 RIGHTUP),否则本轮第一个事件会被系统吞掉。
- **注入器本身要能"自证"**:动作前后各写一行日志(pid/前台窗口标题/光标位置),否则无法区分"没等到"与"按了但没生效"。
- **一轮只启动一次编辑器/PIE**:反复启停既烧时间,本机还会诱发 GPUCrash(见 `UE_EDITOR_LIFECYCLE_AND_HYGIENE.md` §3.5);无人值守验收里,**启动次数要当预算管**。

### 人来当传感器:包装脚本三件套(2026-09-20 实测,"用户飞自由相机定相机端点"那一轮)

当验收仪器就是**用户的眼睛**(构图/手感),不在编辑器里的助手无法注入代码,唯一可行的编排是
"用户操作 + 包装脚本 + 消费式旗标":

1. **包装脚本 = 上游(专家)脚本一字不改地 `exec` + 自己的只读旁录 + 姿势锁**;
   上游脚本读文件必须 `encoding="utf-8-sig"`(BOM 会炸 `compile`)。
2. **可消费旗标**:包装脚本每 tick 轮询 `*_now.flag`,出现 → 执行目标脚本 → **删掉旗标**
   ("再放一次"= 再抓一次,可反复触发,用户不用被重启编辑器)。
3. **姿势锁**:`Alt+C` 进自由相机后 RMB 归自由相机,被测姿势会掉 → 每 tick 把 pawn 的 `Aiming` 写 `True`
   (+ 强制武器网格可见),并留一个 `*_lock_off.flag` 当逃生门。
4. **只读旁录**:每 0.1 s 落一份 live JSON,目标脚本产物一出现就**冻结一份 snapshot**
   —— 这是"抓取那一帧到底发生了什么"的唯一独立证据(本轮靠它抓到 `pc0=DebugCameraController` 的假 0)。
   旁录必须**逐段 try/except**;启动前**清空本轮所有产物和旗标**,否则"读到旧文件"会被误判成成功。
5. **屏幕上的自检文字**:把 candidate 值与策略打在被测画面上(`SystemLibrary.print_string`),
   用户截图即证据;注意别盖住 DebugCamera HUD 自带的 `Loc/Rot`(本轮就盖住了全屏小字,只能靠旁录补)。

> 完整回路(同帧捕获 → 只重启 PIE 的快循环 → 冻结 → 真输入验收 + 5 个失败模式)见
> `ue-vibecoding/reference/HUMAN_LOOP_TUNING.md`。

## 判定"HUD 元素到底有没有画"——PIL 像素扫描(比肉眼可靠)

现象:"代码条件满足、同分支的其他绘制可见,但某个 HUD 元素(速度表/提示)截图里看不到"。
判定法:全屏截图 → PIL 按目标色扫描像素簇 → 看簇的 bbox 是否在预期屏幕区域。
- **FLinearColor 是线性值,显示 sRGB 要换算**:速度表橙色 `(1.0, 0.62, 0.08)` 线性 ≈ RGB(255, 207, 79),别按字面值 (255,158,20) 写扫描条件(踩过,漏检)。
- 扫描出的簇还要排除场景同色物(球体/图标),用"预期区域内的簇数"下结论。
- 结论写法:速度表 = "同 `bEnablesDriveMode` 条件的 Help 文字已切换、目标框正常绘制,但右下角橙/黑像素簇为 0 → 代码路径在跑、绘制无输出"——这种证据链专家才能定位。

### 追一个"用户说看得见、代码说该在中间"的 HUD 元素(2026-09-23 准心一轮,三次失败后定型)

**任务**:用户报"准心不在屏幕中间",而源码里准心坐标写死 `Canvas->SizeX*0.5f, SizeY*0.5f`。要拿到"用户真正看到的那一帧"。

**三次抓图失败(照抄别再踩)**:

| 次 | 做法 | 结果 |
|---|---|---|
| 1 | 带自检开关 `-BBMR116Verify` 启动 + 每帧连拍 | 只落 1 张(game_time=0.389s,**在瞄准窗口之前**)→ 编辑器**崩溃** |
| 2 | 防御式:只在 `pawn.is_aiming()` 为真时截屏,最多 2 张,拿到即注销回调 | **0 张**(超时 9s < PIE 启动 ~7.3s + 瞄准窗口 ~9.8s)→ 编辑器**再次崩溃** |
| 3 | 超时窗放到 16s 重跑 | 脚本**没跑起来**:补丁把续行反斜杠写坏 → `SyntaxError: unexpected character after line continuation character` |

**归因(三栏)**:已排除"准心没画"(`crosshair_draw_count=33`)与"截屏把编辑器弄崩"(第 2 次一张都没截仍崩);
最可能是 **`-BBMR116Verify` 自检(会 `RequestExit`) + PIE + 我注册的 slate tick 回调**三者共存,
回调活过了 PIE 拆解(访问已拆除对象,`EXCEPTION_ACCESS_VIOLATION reading 0x60`)。

**定型配方**:
1. **回调只用来"等编辑器热起来"**:`register_slate_post_tick_callback` 里等到预热秒数 → `editor_request_begin_play()`
   → **同一次 tick 内立刻 `unregister_slate_post_tick_callback`** ⇒ PIE 生命周期内我的代码不再被调用。
   (给用户"可玩"的验收会话就用这个形状:不带自检开关、不截屏、不 `RequestExit`、不 EndPlay。)
2. **抓图别用 `-BBMR116Verify`**;要进瞄准态就直接调游戏自己的 `SetAiming(True)`(`BlueprintCallable`,与 RMB 走同一函数)。
3. **两条独立截图通道**:UE 的 `AutomationLibrary.take_high_res_screenshot` + **OS 层 GDI `CopyFromScreen`**(含窗口边框,能看到 Canvas 之外)。
4. **"看不见"本身就是结论**:4 条独立帧(含 2 条真瞄准态)里准心色像素数 = 0/91(那 91 个是桌面别的内容,不成十字)/0/0
   ⇒ 这比"偏了多少像素"更靠前,先报这个。
5. **canvas ≠ 窗口客户区** 是可测的岔路:DPI 150% 下 `pc.get_viewport_size()` 读到 `[2541,1273]`,而窗口客户区 1706×1018
   —— 想用像素判"居中"必须先说清**在哪个矩形里居中**。

配套纪律:注册了回调的取证脚本**要在被观测生命周期结束前注销**并给回调体加 try 兜底;
给脚本打补丁后必须 `py_compile`(`preflight_lint` 抓不到 `SyntaxError`,第 3 次就是这么白跑的)。

## 无输入驱动的场景机制验证(2026-09-16 实测踩坑,写 C++ 玩法时最省时间的一招)

**要验证的东西**:某个场景机制(滑梯/触发器/吸附)在"球滚进去/贴上去"时是否按设计工作。这种验证不需要人玩,但**不能靠注入速度**。

1. **注入速度无效**:这个项目的球是"输入驱动"的 pawn——`ApplyMovement` 每帧按输入给力、无输入时按 `ReleaseBrakeHz` 指数刹车,还会 `SetPhysicsLinearVelocity`。远端注入 `set_physics_linear_velocity(600)` 实测**只挪了 5cm**(3s 真实时间)。刚体睡眠也会吃掉速度。
2. **正确姿势:加一个"世界方向强制驱动"调试钩子**(本仓库已有 `bDebugAutoDriveForward` 走驱动分支,再加 `DebugAutoDriveWorldDir` 覆盖驱动方向为世界向量):
   - `Desired = Forward*MoveInput.Y + Right*MoveInput.X;` 之后 `if (!DebugAutoDriveWorldDir.IsNearlyZero()) Desired = DebugAutoDriveWorldDir;`
   - 于是"把球放在高台上、让它朝圆弧滚"变成两条属性设置,可反复复现,还能把同样的钩子交给用户/队友自测。
   - 注意:**绕过输入投影会让球"顶着支撑面推"**,比如球落在墙上后仍被往墙里推 → 出现"冷却一过就被反向吸回"的假 ping-pong。判断真实性看:真实 WASD 方向是投影到支撑面上的(`Forward -= Up*dot(Forward,Up)`),推不进面。
3. **PIE 怎么起**:`LevelEditorSubsystem.editor_play_simulate()` 是 **Simulate 模式 = 给你一个 SpectatorPawn、根本不生成小球**(`get_all_actors_of_class(GSRollingBallPawn)` 返回空)。要真的球,用 `editor_request_begin_play()`(不是 `editor_play_in_editor`,后者在本版 Python 里不存在);`is_in_play_in_editor()` 会滞后一拍才是 True,给它 ~20s。
4. **编辑器在后台会被深度节流**:3s 真实时间里 PIE 只推进约 0.1s 游戏时间。跨脚本观测一律"发射 → 真实 sleep 15~40s → 读",别用短 sleep 下结论。
5. **`log` 是主证据**:把逐帧日志写在子步级(开始/每 0.1s/释放)比任何采样都可靠;`grep -a`(日志含二进制字节)读 `Saved/Logs/<项目>.log`,注意 grep 到的是**历史累积**,对比前后行数才知道本轮新增了什么。

## UE 5.8 Python API 小坑(本轮新踩)

- `HitResult` **没有直接字段**(`r.impact_point` 报 AttributeError)→ 用 `r.to_dict()`(键:`blocking_hit`/`impact_point`/`impact_normal`/`hit_actor`/`hit_component`)或 `get_editor_property`。
- `SceneComponent` 没有 `get_component_location()` → 用 `get_world_location()`。
- `PlayerController` 没有 `get_pawn()`；**`get_editor_property('pawn')` 也不通**（2026-09-26 实测：
  `Property 'Pawn' for attribute 'pawn' on 'PlayerController' is protected and cannot be read`）
  → 取 pawn 用 `unreal.GameplayStatics.get_player_pawn(world, 0)`（可读），或 `pc.call_method("K2_GetPawn")`。
- `unreal.Vector` 没有 `.size()`/`.size()` → 手算 `((a-b).x**2+...) ** 0.5`。
- `get_all_actors_of_class(w, unreal.StaticMeshActor)` **漏掉蓝图派生的 actor**(如 `Blockout_Corner_Curved_C`)→ 找组件/找特定 actor 一律枚举 `unreal.Actor` 再 `get_components_by_class(...)`。
- 关卡 actor 上的组件属性:`c.get_editor_property('X')` / `c.set_editor_property('X', v)`,改完 `unreal.EditorLevelLibrary.save_current_level()` 返回 True/False 要打印出来核对。

## 关卡几何速查:用射线画剖面

不知道"球会滚到哪一面/往哪个方向滚"时,别猜——在编辑器世界(`EditorLevelLibrary.get_editor_world()`)里扫射线:

- 竖扫(找平台顶面/圆弧起点):`line_trace_single(w, (x,y,-250), (x,y,-900), TraceTypeQuery.ECC_VISIBILITY, True, [], DrawDebugTrace.NONE, True)`,沿 y 每 10~20cm 一次,打印 `impact_point.z` + `impact_normal` + actor 名,就能看出"平面 → 圆角(法线在转)→ 竖直面"的剖面和圆角半径。
- 横扫(找竖直面):固定 z,从远处朝面打,法线 (0,±1,0) 即竖直墙,命中 y 就是墙面位置。
- 这比在 PIE 里"试出来"快一个数量级;本文的 90° 圆角几何(A 面 z=−500 / B 面 y=−1200 / R≈111)就是 3 次扫描画出来的。

## 编辑器内 Python 驱动 PIE 的定型配方（2026-09-26 实机导出轮，零 C++ 改动）

**用途**：需要"进 PIE 拿实机状态"（相机 / 网格组件 / 角色状态 / 资产尺度）而又**不想改 C++、不想重编**时用这条。

**启动**（PowerShell，一次会话；开之前先过 `ue-cpp-build-cnpath` 步骤 G 的两道硬门）：

```text
UnrealEditor.exe "<proj>.uproject" "/Game/<Map>.<Map>" -windowed -ResX=1280 -ResY=720 \
  -nosound -NoSplash "-abslog=<日志文件>" "-ExecCmds=py <脚本绝对路径>"
```

- **`-ExecCmds` 里的路径含空格时必须整体加引号**；`Start-Process` 传数组**不会**替你加引号，会把它拆成两个参数
  ⇒ 参数串自己拼好、整体交给 `-ArgumentList`。
- **`-abslog=` 才落盘**（`-log=` 只是切屏幕日志）。收尾用 `Start-Process -PassThru` 拿 PID，
  只杀**自己那个 PID**（按进程名会误杀别人的编辑器）。

**脚本骨架**（相位机；不要在主线程里 sleep）：

```python
import unreal, time, json
S = {"phase": "boot", "busy": False, "t0": time.time()}

def tick(dt):
    if S["busy"]:                 # 重入闸：tick 期间可能被嵌套触发
        return
    S["busy"] = True
    try:
        if S["phase"] == "boot":
            if time.time() - S["t0"] >= 8.0:            # 等地图加载完再起 PIE
                ss = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
                S["pie_before"] = bool(ss.is_in_play_in_editor())
                ss.editor_request_begin_play()           # 返回 void！见下
                S["phase"] = "pie"
        elif S["phase"] == "pie":
            w = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
            if w is None:
                return                                   # 继续等
            pc = unreal.GameplayStatics.get_player_controller(w, 0)
            pawn = unreal.GameplayStatics.get_player_pawn(w, 0)   # ← 最可靠的取 pawn 途径
            ...                                          # 校验/采样/写盘 → finish()
    finally:
        S["busy"] = False

handle = unreal.register_slate_post_tick_callback(tick)
```

**收尾**：`unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()`，
再 `unreal.SystemLibrary.quit_editor()`（拿不到就走启动器的 PID 兜底杀）。

**四条实测要点**：

1. **`editor_play_simulate()` ≠ `editor_request_begin_play()`**：前者是 Simulate（给你 SpectatorPawn、
   不生成玩家 pawn），要"有玩家 pawn 的 PIE"必须用后者；`is_in_play_in_editor()` 用来判状态。
2. **void 返回不是失败**：`editor_request_begin_play()` 返回 `None` —— 把 "returned None" 当失败写进探测报告，
   会让你以为"PIE 没起来"而在下一次会话里白等（本项目实测白等 120 s）。判据应是"**没抛异常**"。
3. **接受一个"找到的对象"前，先要求它四件套齐备**（网格 / 弹簧臂 / 相机 / 胶囊等按任务定）：
   否则 SpectatorPawn、代理 pawn、空 pawn 都会被你当成目标，后面全部字段变 None。
   实测做法：候选来源按序尝试（`GameplayStatics.get_player_pawn` → `PC.call_method("K2_GetPawn")` → …），
   每个候选都做"组件齐备性"检查，**接受第一个齐备的**，并把这个来源名写进报告。
4. **写盘要有存在性硬判**（`os.path.isfile` + 非空 + 断言），并且**每次相位结束时增量落盘**：
   Python 里抛异常时 UE 进程仍可能 exit=0，"我写了 JSON"不能靠返回值相信。

**"哪些量只能从 PIE 拿"**：相机世界变换、网格组件世界变换（component space 的参考系）、
角色的实时速度/是否着地、资产运行期尺度与胶囊参数 —— 这些在编辑器（非 PIE）里读不到或读到的是默认值。

## 2026-10-06 补充：**在用户开着的编辑器里**用组播远程执行跑多轮 PIE（不重启编辑器）

- 入口：`python reference/ue_pyexec.py "<代码>" --timeout 240`
  （MCP 没起也能用；长代码自动写临时文件走 `ExecuteFile`；`print` 会回传到客户端）。
- 脚本里 `unreal.register_slate_post_tick_callback(tick)` 注册的状态机会**在本次调用返回后继续在编辑器里跑**，
  所以"两轮 PIE（改前测 → 改 → 改后测）"可以一次发射完成，结果写文件轮询读取。**必须有 420s 看门狗**，
  超时也要把已有结果写盘（否则一次卡死 = 报告全丢）。
- **先保存用户的 session 再动手**：`EditorLoadingAndSavingUtils.get_dirty_map_packages()` →
  `set_current_level_by_name(<L>)` + `save_current_level()` 逐个保存（当天就是这样先落盘了用户手动删白盒的
  Level1/Level2/Level3，`dirty=[]` 之后才改东西）；改前/改后各拷一份磁盘地图做双备份。
- **三个当天踩到的状态机坑**：
  1. **跨轮不能复用 Pawn 句柄**：`editor_request_end_play()` 后旧 Pawn 已销毁，下一轮再 `set_actor_location`
     会抛 `GSRollingBallPawn: Internal Error - ObjectInstance is null!`，被 try 吞掉 → 每帧空转直到看门狗。
     **每次进入新一轮 PIE 都要 `pawn = None` 重新取。**
  2. **`end_play()` 之后不要立刻枚举世界/保存**：世界还在 teardown，`get_all_level_actors()` 返回空、
     `save_current_level()` 返回 False（当天因此"销毁 0 个代理"。等 4~5s 或先 `load_level` 再等 2s）。
  3. **同一脚本被跑两次会重复建对象**（代理/玩家起点）：写入前先按标签查重。
- **真实小球 > 射线**：地板/碰撞验收用"传送真实 Pawn + 等 1.25s + 看 z"，判据 `z < 起点-260` 记为掉；
  射线（哪怕 `ECC_PHYSICS_BODY=BLOCK`）**不能**证明滚球踩得住（平面网格只有复杂碰撞时会穿过）。
- 用户在场时 PIE 会占用他的视口：跑短一点（每点 1.25s、一轮 ≤20 点），跑完 `end_play` 把界面还回去。
