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
