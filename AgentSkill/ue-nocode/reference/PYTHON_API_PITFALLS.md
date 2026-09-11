# 编辑器内 Python API 坑速查

> 什么时候读这个文件:`get_editor_property` 抛异常、类加载返回 None、枚举传参报错、脚本行为和预期不符。

## 属性不存在(改用方法/别的对象)

- **C++ UPROPERTY 的 bool `b` 前缀在 Python 里被剥掉**:`bLookAlongNegativeAxis` → `look_along_negative_axis`,`bSnapToGrid` → `snap_to_grid`。`set_editor_property('b_xxx', ...)` 会抛 "Failed to find property"(2026-09-03 实踩);同名字段先按去 b 前缀猜
- **`unreal.Vector2D(a, b)` 参数序是 (X, Y)**:`set_move_input(Vector2D(0,-1))` 是"S 后退"不是"A 左移"(X=左右/A-D,Y=前后/W-S)。传反了测试结果"看起来像代码错了",2026-09-03 测墙面控制时白白浪费一轮排查——先打印/推敲再下结论
- `StaticMeshComponent` **没有** `collision_enabled`/`collision_profile_name`/`use_ccd` 属性 → 用方法 `get_collision_enabled()`/`get_collision_object_type()`/`set_use_ccd(bool)`;或读 `get_editor_property("body_instance")`(BodyInstance 里才有 collision_enabled/object_type/collision_responses)
- `StaticMesh` **没有** `collision_trace_flag`,它在 `mesh.get_editor_property("body_setup")` 上(建模网格碰撞根因见 PROJECT_SETUP.md)
- `HitResult` 在本版本**不是 subscriptable**,用 `.to_dict()`
- `EditorStaticMeshLibrary` 没有 `get_number_simple_collisions`;整个 EditorScriptingUtilities 已废弃(DeprecationWarning),优先用 Subsystem

## 类与蓝图

- 蓝图类加载:`unreal.load_asset(path).generated_class()`,**别用** `unreal.load_class(None, path)`——后者本环境偶尔返回 None
- 查蓝图父类:**`unreal.Blueprint.get_blueprint_parent_class(bp)`**;`generated_class().get_super_class()` 会 AttributeError(且异常被吞时引发误判连锁)
- Key 结构体直接打印是空 `{}`;真实键名用 `key.get_editor_property('key_name')` 读
- 顶层工具 `execute_python_code` **异常时吞 output** 只回 error_message → 重逻辑拆步或整体 try/except

## 枚举与函数签名

- Python 侧枚举**不带 E 前缀**:`unreal.GSGravityChangeReason.SCRIPTED` ✓;`unreal.EGSGravityChangeReason` 不存在,原始 int 也不接受
- UFUNCTION 签名靠 **TypeError 提示逐个补参**:实测 `request_toggle_gravity(requester, reason, force)` 报了三次错才凑齐;参数名snake_case(`block_profile`/`volume_extent`/`gravity_revision`)
- 属性名猜不中就 `dir(obj)` 找方法 + 逐个试 `get_editor_property`
- `find_actors` 必填 `name`/`tag`/`collision_channels` 全套(空传 `tag:""`, `collision_channels:[]`);`set_actor_transform` 参数名是 **xform**
- `EditorLevelLibrary` 大量方法带 DeprecationWarning,能用就用(不影响功能),优先 Subsystem 写法:`unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)` / `unreal.UnrealEditorSubsystem` / `unreal.EditorActorSubsystem`

## 世界与关卡

- 编辑器世界:`unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()`;PIE 世界:`get_game_world()`(仅 PIE 期间非 None)
- World 对象没有 `get_time_seconds`/`time_seconds` 属性;游戏时钟用 `actor.get_game_time_since_creation()` 或 `GameplayStatics.get_game_time_in_seconds`(后者本绑定可能没有,前者实测可用)
- **当前关卡可能是未保存的 `/Temp/Untitled_N`**——任何依赖关卡的脚本先显式打开目标关卡并断言:
  ```python
  import unreal
  sub = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
  sub.load_level("/Game/测试案例")            # 包路径,不带 .umap;先确认磁盘上存在
  print(sub.get_editor_world().get_name())     # 断言
  ```
- `get_all_level_actors` 在编辑器繁忙/PIE 期间可能返回陈旧或不完整数据;先打类直方图再找目标,必要时用选中集

## Bash → Python 传递坑(MSYS)

- `grep` 模式以 `/` 开头(如 `/Game/...`)被 MSYS 当路径转换报错 → 去掉开头斜杠写 `Game/...`
- bash 双引号里给 `python -c "..."` 传脚本:`$var` 会被 bash 先展开吃掉 → PS/Python 内联脚本里的 `$` 必须转义,或者干脆写成临时文件执行
- 中文路径/中文字符串经 bash 传给 python 可能被编码层搅乱(出现过 GBK↔UTF-8 互转的乱码文件名)→ 文件操作用 python 的 os.listdir/os.rename 处理,别在 bash 里写中文字面量
- `cmdkey /list` 在 Git Bash 报"命令行参数不正确"(`/list` 被转成路径)→ 改用 PowerShell
- WindowsApps 的 python.exe 本机是真 Python 3.13,不是商店假指针,直接用
- Windows Python 看不到 `/tmp`(MSYS 虚拟路径)→ 用真实 Windows 路径

## 输入模拟(pawn)

- `SetMoveInput(Vector2D)` 是 BlueprintCallable 可脚本驱动,但 **`PollNativeInput` 每 tick 用键盘实况覆盖 MoveInput**——脚本验证前必须 `ball.set_editor_property('enable_native_polling_input', False)`,测完恢复 True
- `move_input` **不反射到 set_editor_property**(2026-09-08 实测:`set_editor_property('move_input', ...)` 直接抛 "Failed to find property")——必须调方法 `ball.set_move_input(unreal.Vector2D(0, 1))`;Vector2D(x, y) Y=前
- 验证方向:相机前向投影到支撑面 → `dot(velocity, fwd)` 正=W 正确;`up.cross(fwd)` 为右,A 应得负点积

## 存盘

- `EditorAssetLibrary.save_asset` 对某些改动返回 False(假拒,值其实只在内存)→ 改用**静态工具类** `unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`(注意它是工具类**不是 Subsystem**,`get_editor_subsystem` 会报 "must be a Class");存完用磁盘 mtime 确认真的落盘

## 类名与函数调用(2026-09-05 实踩)

- **Python 类名没有 A/U 前缀**:`AGSKey`/`AGSPickupItem`/`AGSDoor` 在 unreal 模块里是 `unreal.GSKey`/`unreal.GSPickupItem`/`unreal.GSDoor`;`hasattr(unreal,'AGSKey')` 恒 False、`getattr` 返回 None,别误判成"类没编译进去"。断言新类用去前缀名
- **BlueprintPure 函数 ≠ 属性**:`IsMessageLocked()` 这类 BlueprintPure 要方法调用 `ball.is_message_locked()`,`get_editor_property('is_message_locked')` 抛 "Failed to find property"。判据:UFUNCTION 标 BlueprintPure 的用括号,UPROPERTY 的用 get/set_editor_property


## CDO/调试标记的坑(2026-09-08 实踩,2026-09-11 补根因)

- **改 CDO 不会传给 PIE 实例(根因已查明:引擎安全层直接拒绝 Python 写 CDO)**:`get_default_object()` 赋值报 `Blocked unsafe Python code: get_default_object() modification. Modifying Class Default Objects (CDOs) from Python causes crashes.`(本机 2026-09-11 实测)。这也解释了更早一轮「CDO 读回 True 但 PIE 实例读到 False」的谜团——写入根本没生效。调试开关一律在**实例**上 set_editor_property;想要「PIE 里能翻」的开关就做成 Tick 逐帧读的属性
:`unreal.get_default_object(unreal.XXX)` 写入属性后,CDO 读回 True,但 PIE 新生成的实例读到 False(原因未查明,两处 CDO 都试过)。实用规则:想让 PIE 里的开关立即生效,把它做成 **Tick 逐帧读**的属性,PIE 里直接 `ball.set_editor_property(...)` 改实例;BeginPlay 一次性消费的属性没法运行时翻
- `SceneComponent.set_using_absolute_location` **没暴露给 Python**——不能运行时翻转,只能编译期/BeginPlay 决定
- **没有全局 `unreal.load_blueprint_class`**,要用 `unreal.EditorAssetLibrary.load_blueprint_class("/路径/资产名")`
- 改 CDO 会把 BP 资产弄脏,autosave 会把调试默认值写进 .uasset → 本次 BP_GSRollingBallPawn.uasset 差点带着调试标记混进提交(amend 才摘掉)。**commit 前 `git status --short` 审查:凡是你没打算改的 .uasset/.umap 出现,一律 `git checkout <干净提交> -- <文件>` 恢复再提交**
- `EditorAssetLibrary.reload_asset` 不存在;防再污染靠"改完立即复位 CDO 标记 + git 审查"

## 2026-09-08 手感/校准轮新坑

- **`EditorAssetLibrary.load_asset(路径)` 可能返回 None 而 `unreal.load_object(None, '路径.对象名')` 能拿到**——新启动的编辑器资产注册表未扫描完。DataAsset 等加载不进来时改用 load_object(全名带 `.对象名` 后缀)
- **改 DataAsset 属性后不会自动标脏**:set_editor_property 之后用 `EditorLoadingAndSavingUtils.save_dirty_packages(True, True)` 落盘,并用**磁盘 mtime 对比当前时间**确认真的写了(假拒的判据)。`save_loaded_asset`/`mark_package_dirty` 均不存在
- **PIE 游戏世界里 `EditorActorSubsystem.spawn_actor_from_class` 返回 None**(只在编辑器世界可用);`GameplayStatics.begin_spawning_actor_from_class` 没暴露。临时测试台的正确做法:**PIE 启动前在编辑器世界摆好**(PIE 会复制),或先 end_play 在编辑器世界摆好再开 PIE
- `SceneComponent` 读世界位置用 `get_world_location()`(没有 get_actor_location);`PlayerController.get_viewport_size()` **不收参数**、直接返回 (宽, 高) 元组
- UE 深度后台节流会污染"终端速度"类读数:采样间隔按真实时间算不准游戏时间——**标定读数一律用位置+速度联合探针**(位置位移/速度对照),单看速度全是噪声

## 给已有关卡 Actor 挂组件 / 形状探针(2026-09-11 转向器轮)

- `Actor.add_component_by_class` 本版本**没有**;给已摆放的 Actor 挂组件的正确姿势是 Subobject 子系统(编辑器"细节面板 +Add Component"同款):
  ```python
  sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
  handles = sds.k2_gather_subobject_data_for_instance(actor)   # [0] = 根
  par = unreal.AddNewSubobjectParams()
  par.set_editor_property('parent_handle', handles[0])
  par.set_editor_property('new_class', unreal.GSRedirectorComponent)
  h, fail = sds.add_new_subobject(par)
  comp = actor.get_components_by_class(unreal.GSRedirectorComponent)[0]
  ```
  加完 `LevelEditorSubsystem.save_current_level()` 落盘;实例组件随关卡(umap)保存,编辑器重启后仍在(实测)。关卡 Actor 数据存在 umap 里还是 `__ExternalActors__` 里,`git status` 一看便知(本次全在 umap)。
- `line_trace_multi` 的 python 简写不行(第 9 个位置参数被当成 TraceColor 结构报 NativizeProperty 错)→ 用 `line_trace_single` **链式续打**(命中点沿方向推进 3cm 再打),等价多命中。
- **"某点能不能放半径 R 的球"用球面探针判定**:`sphere_trace_single(start=p, end=p+1cm, radius=R, trace_complex=True)` 读 `to_dict()['initial_overlap']`——比拿三角面数据重建形状省事得多(栅格扫一遍就是"球心可行空间"地图;关卡的坡度/滑梯内径/通道宽度就是这么量出来的)。
- 编辑器里改**资产**物理碰撞:5.8 的枚举名是 `unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE`(复杂碰撞当简单用,三角面直接参与物理);`mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', ...)` + `EditorAssetLibrary.save_loaded_asset(mesh)`,PIE 复制世界时会重新注册生效。
