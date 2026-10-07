# 编辑器内 Python API 坑速查

> 什么时候读这个文件:`get_editor_property` 抛异常、类加载返回 None、枚举传参报错、脚本行为和预期不符。

## 属性不存在(改用方法/别的对象)

- **C++ UPROPERTY 的 bool `b` 前缀在 Python 里被剥掉**:`bLookAlongNegativeAxis` → `look_along_negative_axis`,`bSnapToGrid` → `snap_to_grid`。`set_editor_property('b_xxx', ...)` 会抛 "Failed to find property"(2026-09-03 实踩);同名字段先按去 b 前缀猜
- **`unreal.Vector2D(a, b)` 参数序是 (X, Y)**:`set_move_input(Vector2D(0,-1))` 是"S 后退"不是"A 左移"(X=左右/A-D,Y=前后/W-S)。传反了测试结果"看起来像代码错了",2026-09-03 测墙面控制时白白浪费一轮排查——先打印/推敲再下结论
- `StaticMeshComponent` **没有** `collision_enabled`/`collision_profile_name`/`use_ccd` 属性 → 用方法 `get_collision_enabled()`/`get_collision_object_type()`/`set_use_ccd(bool)`;或读 `get_editor_property("body_instance")`(BodyInstance 里才有 collision_enabled/object_type/collision_responses)
- `StaticMesh` **没有** `collision_trace_flag`,它在 `mesh.get_editor_property("body_setup")` 上(建模网格碰撞根因见 PROJECT_SETUP.md)
- `HitResult` 在本版本**不是 subscriptable**,用 `.to_dict()`;且**没有 `.actor`/`.hit_actor` 属性**(直接访问 AttributeError)——dict 里读 `['hit_actor']`(Object)/`['location']`/`['normal']`(Vector 结构体,`.z` 可读)/`['distance']`(2026-09-13 实踩)
- `EditorStaticMeshLibrary` 没有 `get_number_simple_collisions`;整个 EditorScriptingUtilities 已废弃(DeprecationWarning),优先用 Subsystem

## 类与蓝图

- 蓝图类加载:`unreal.load_asset(path).generated_class()`,**别用** `unreal.load_class(None, path)`——后者本环境偶尔返回 None
- 查蓝图父类:**`unreal.Blueprint.get_blueprint_parent_class(bp)`**;`generated_class().get_super_class()` 会 AttributeError(且异常被吞时引发误判连锁)
- Key 结构体直接打印是空 `{}`;真实键名用 `key.get_editor_property('key_name')` 读
- 顶层工具 `execute_python_code` **异常时吞 output** 只回 error_message → 重逻辑拆步或整体 try/except

## 枚举与函数签名

- Python 侧枚举**不带 E 前缀**:`unreal.GSGravityChangeReason.SCRIPTED` ✓;`unreal.EGSGravityChangeReason` 不存在,原始 int 也不接受
- UFUNCTION 签名靠 **TypeError 提示逐个补参**:实测 `request_toggle_gravity(requester, reason, force)` 报了三次错才凑齐;参数名snake_case(`block_profile`/`volume_extent`/`gravity_revision`)
- **签名速查优先 `__doc__` 自省**:`python ue.py py "print(unreal.X.method.__doc__)"` 一次拿到参数名+类型(实测 `set_level_viewport_camera_info(camera_location, camera_rotation, viewport_config_key)`);MCP 的 `discover_python_function`/`discover_python_class` 是**顶层工具、不属于任何 toolset**,`ue.py call <toolset> ...` 调不到("Toolset not found"),别绕路
- `LevelEditorSubsystem.set_level_viewport_camera_info(loc, rot, viewport_config_key)`:5.8 **第三参必填**,传 `''` 即可;不传报 TypeError required argument not found(2026-09-13 实踩)
- `TraceTypeQuery.TRACE_TYPE_QUERY1` 已废弃(DeprecationWarning)→ 用 `unreal.TraceTypeQuery.ECC_VISIBILITY`;`line_trace_single(w, start, end, channel, complex, [], DrawDebugTrace.NONE, True)` 的 `actors_to_ignore` 传 `[]`
- `actor.get_folder_path()` 返回 **`unreal.Name`**(没有 `.path`)→ `str()` 直接用;配 `get_actor_label()` 读标签——**大纲文件夹+标签是关卡侧唯一可靠的分组信息**(P1/P2 这种),umap 二进制里拿不到
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
- **`py_compile` / 任何把路径当参数的 Python 调用,都要传 Windows 风格路径**:写成 `py_compile /d/rigwork_ue/x.py`,Windows Python 会去找 `<当前盘>:\d\rigwork_ue\x.py` → `No such file or directory`(而文件其实在,极易误判成"文件没生成")。正确写法 `py_compile "D:/rigwork_ue/x.py"`。
- **反过来**给 exe 传参时(如 `UnrealEditor.exe`)用 `D:/...` 正斜杠最稳;只有 `@rsp` 这类 cl/link 前缀参数才必须 `cygpath -w` 转反斜杠(见 `ue-cpp-build-cnpath` §3b)。

## 输入模拟(pawn)

- `SetMoveInput(Vector2D)` 是 BlueprintCallable 可脚本驱动,但 **`PollNativeInput` 每 tick 用键盘实况覆盖 MoveInput**——脚本验证前必须 `ball.set_editor_property('enable_native_polling_input', False)`,测完恢复 True
- `move_input` **不反射到 set_editor_property**(2026-09-08 实测:`set_editor_property('move_input', ...)` 直接抛 "Failed to find property")——必须调方法 `ball.set_move_input(unreal.Vector2D(0, 1))`;Vector2D(x, y) Y=前
- 验证方向:相机前向投影到支撑面 → `dot(velocity, fwd)` 正=W 正确;`up.cross(fwd)` 为右,A 应得负点积

## 存盘

- `EditorAssetLibrary.save_asset` 对某些改动返回 False(假拒,值其实只在内存)→ 改用**静态工具类** `unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`(注意它是工具类**不是 Subsystem**,`get_editor_subsystem` 会报 "must be a Class");存完用磁盘 mtime 确认真的落盘

## 类名与函数调用(2026-09-05 实踩)

- **Python 类名没有 A/U 前缀**:`AGSKey`/`AGSPickupItem`/`AGSDoor` 在 unreal 模块里是 `unreal.GSKey`/`unreal.GSPickupItem`/`unreal.GSDoor`;`hasattr(unreal,'AGSKey')` 恒 False、`getattr` 返回 None,别误判成"类没编译进去"。断言新类用去前缀名
- **BlueprintPure 函数 ≠ 属性**:`IsMessageLocked()` 这类 BlueprintPure 要方法调用 `ball.is_message_locked()`,`get_editor_property('is_message_locked')` 抛 "Failed to find property"。判据:UFUNCTION 标 BlueprintPure 的用括号,UPROPERTY 的用 get/set_editor_property
  - ⚠️ **但有个例外:同名 UPROPERTY 会遮蔽函数**(2026-09-18 补,见文末"属性遮蔽"节)。
    这段结论成立的前提是"没有同名的 `bXxx` 属性";C++ 里若同时写了 `bool bFoo` 和 `bool IsFoo()`,
    Python 侧 `obj.is_foo` 是**属性**不是方法,再按这条去加括号会炸。


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

## 2026-09-14 v5 验收轮:几个"想当然存在"的 API 其实没暴露

三条都是 C++ 有、Python 没有(报 AttributeError),替代写法记牢,别再逐个试错:

| 想用 | 实际 | 替代 |
|---|---|---|
| `pawn.get_character_movement()` | ✗ 不存在 | `pawn.get_component_by_class(unreal.CharacterMovementComponent)`,再读 `get_editor_property('velocity')` / `('movement_mode')`;`is_moving_on_ground()`/`is_falling()` 在组件上可用 |
| `comp.get_relative_location()` / `get_relative_rotation()` | ✗ 不存在 | `comp.get_editor_property('relative_location')` / `('relative_rotation')`(组件与 Actor 的 root_component 都适用) |
| `world.get_time_seconds()` | ✗ 不存在 | `unreal.SystemLibrary.get_game_time_in_seconds(world)`(膨胀下给的是**游戏时间**,做角速度/速率换算直接可用) |

- 顺带:`actor.get_velocity()` 在 Actor 上**存在**(恒 0 或只反映角色运动),别拿它判断被投掷物的松散模拟速度——
  那种速度存在工具自己的 `LooseVelocity`(protected,未暴露),验收只能靠"位置差/姿态差 ÷ 游戏时间差"来算。
- **CUA 坐标点击绑定"最近一次返回的光栅"**:先 zoom 再 click 会报 `frame pixel coordinate (x,y) is outside 140x30`——
  zoom/crop 之后必须重新 `screenshot` 再点。
- **`rg_shot.py` 裁剪坐标是客户区物理像素**(本机 1712x1024);从 CUA 光栅(整屏缩放成 1280 宽)换算 ≈ ×1.3375,
  别再按窗口 bounds(2569)去乘。
- **右键按住**优先 `ctypes` 直接 `mouse_event(0x0008,0,0,0,0)`(不移动光标);旧的 SendInput 绝对坐标版
  (`MOUSEEVENTF_ABSOLUTE` 里塞像素值)会把光标甩到屏幕角落、可能带偏相机。

## 2026-09-15 再三条(外部包验收轮)

- **`runpy.run_path(p)` 的 `__name__` 是 `<run_path>`,不是 `__main__`**:脚本结尾写成
  `if __name__ == "__main__": build()` 时,`run_path` 只执行模块级定义,**build 静默不跑**(无报错、无输出,
  最容易被误判为"脚本跑过了")。正确写法:`ns = runpy.run_path(p); ns["build"]()`。
- **`Automation RunTests <路径>` 会先把 PIE 结束掉**:真机日志里 `Test Started` 之后紧跟
  `LogWorld: BeginTearingDown for /Game/Maps/UEDPIE_0_...`,随后测试报"请先进入 PIE"。
  → "先起 PIE 再跑集成测试"的文档跑法在真机不成立;要自带 `FStartPIECommand` 的自包含测试,
  或降级为不依赖 PIE 的规则测试。
- **自动化测试要求 ≥10 FPS**:编辑器在后台被深度节流(实测 3 FPS)时会卡在
  `FWaitForInteractiveFrameRate: Waited 30 seconds ... Current FPS=3`(最长等 600 秒)。跑测试前把编辑器切前台。

## 2026-09-15 V15 轮:引擎内验证 ProceduralMesh 几何(5 次迭代才跑通,全是"想当然存在"的 API)

场景:项目 C++ 类(如 `RGCombatMeshComponent`,派生自 ProceduralMeshComponent)在运行时把 `.inl`
常量数组嵌成网格,要在编辑器里**取回几何**验证绕序/体积。逐条坑:

1. **`actor.add_component_by_class` 没有** → 用 `unreal.new_object(unreal.XxxComp, outer=actor)` 创建;
   但 `add_instance_component`/`register_component` 在 5.8 **也未暴露** → 干脆**不把组件注册到 Actor**,
   孤儿组件照样能建网格、照样能读 section,验证目的够用(别在注册上耗时间)。
2. **`unreal.KismetProceduralMeshLibrary` 不存在** → 正确类名是
   **`unreal.ProceduralMeshLibrary`**(C++ 类的 ScriptName 元数据剥了 Kismet 前缀);
   拿不准时 `hasattr` 探测两个名字。
3. **`get_section_from_procedural_mesh(comp, section_index)` 返回三角形是
   扁平 int 列表**(不是 Triangles 结构体列表):`(vertices, triangles, normals, uvs, ...)` 里
   `triangles` 按 3 个 int 一组切就是三角索引;顶点是 `unreal.Vector` 列表可直接算
   `dot(法线, 几向外向)` 与有符号体积。
4. **`spawn_actor_from_class` 只在编辑器世界可用**(PIE 世界返回 None,见 2026-09-08 条);
   验几何根本不需要 Actor——`new_object` 出组件、`create_mesh_section` 喂数据、读回,三步完事。
5. 通用教训:**这类验证脚本一次写对的概率≈0**,把"建组件→喂数据→取回→断言"拆成
   每步 `print` 落盘($env:TEMP 文件,PowerShell 不回传 stdout),报错定位在第几步;
   预期要 3~5 轮迭代,别按一次成功排期。

## 命令行(commandlet)模式与"建图两段式"的实测修正 (2026-09-18, BaoBiaoStreet)

`UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script=x.py -unattended -nosplash -NullRHI -NoSound -UTF8Output`

- **开机世界是 `Untitled`, 不读 `EditorStartupMap`**。所以"靠 ini 指定启动图"在 commandlet 里不成立,
  必须在脚本里显式 `load_level()` 或 `new_level()`。
- **但 `new_level()` / `load_level()` 在 commandlet 里是同步生效的**: 调完立刻
  `get_editor_world().get_name()` 就是目标关卡名。于是"建图 + 摆场景 + 存盘"**可以在同一个进程里顺序做完**,
  不必强制拆成两个进程(技能里"建图/切图必须两段式"那条是**编辑器交互模式**下的结论)。
- **退出码恒为 1**: 日志末尾 `LogGameFeatures: Error: Asset manager settings do not include a rule for
  assets of type GameFeatureData` → `Failure - 2 error(s)`, 这是模板噪音, 脚本已成功执行。判成功要看
  `LogPythonScriptCommandlet: Python script executed successfully` + 自己 `unreal.log` 的 marker。
- `unreal.log()` 在 commandlet 里**不进 stdout**, 去 `<项目>/Saved/Logs/<Project>.log` 里 grep。
  (stdout 只有 `-Cmd` 的引擎日志, grep 自己的 marker 要去项目日志)

## 骨骼/网格朝向怎么读 (UE5.8 Python 实测可用集)

- `SkeletalMeshComponent` **没有** `get_bone_location` / `get_local_bounds`(5.8 未导出)。
- **能用且好用**: `SkeletalMesh.get_imported_bounds()` / `get_bounds()` → FBoxSphereBounds
  (`origin` + `box_extent`, 本地空间, **资产级不会像 actor bounds 那样读到未初始化的陈旧值**)。
- `Skeleton.get_reference_pose()` 返回 **`AnimPose` 对象**(不是数组), 成员有
  `get_bone_names` / `get_bone_pose` / `get_ref_bone_pose` / `get_ref_pose_relative_transform` /
  `get_relative_transform` / `get_socket_pose`。
- `Skeleton.get_editor_property('bone_tree')` → `FBoneNode[]`; **`Name` 是 protected 读不出来**,
  用 `AnimPose.get_bone_names()` 取骨头名。
- **判定朝向的低成本硬办法**: 对比"资产级 bounds"的**不对称性**。
  例: VRoid 角色脑后头发多 → 前后不对称; 拿 Blender 侧已知 bounds 和 UE 侧对上, 就能反解坐标映射。

## Blender <-> UE 的轴向映射 (glTF/VRM -> Blender -> FBX -> UE, 实测)

用 Blender 默认 FBX 导出 (`axis_forward='-Z'`, `axis_up='Y'`, `global_scale=1.0`,
`bake_space_transform=False`) 再进 UE, 实测映射是:

```
Blender (x, y, z)  ->  UE (x, -y, z)
```

即 **Blender 的 -Y(角色正前) 变成 UE 的 +Y**, **不是** UE 惯例的 +X。
所以要"面朝 UE +X"得给 Actor **yaw = -90**; "面朝 -X" 用 **yaw = +90**
(UE yaw 公式: (x,y) -> (x cosθ - y sinθ, x sinθ + y cosθ))。
`global_scale=1.0` 时高度正确到 cm(实测 1.645 m 模型 -> UE 高 164.5), 不会出现 100 倍炸裂。

## VRM/VRoid 导入的两个细节

- VRM 是 glTF 容器, **改后缀 `.vrm` -> `.glb` 就能进 UE glTF 导入器**(MToon 会退化成 PBR,
  贴图照常带过来)。`unreal.GLTFImportFactory` 在 5.8 的 Python 里**不存在**, `AssetImportTask.factory`
  留空让 Interchange 自己挑即可。
- VRM 里会带**无父级的脚手架网格**(如 secondaryAnimation 用的 42 顶点 `Icosphere`, 半径 1m),
  Blender 里按"有父级才算角色"过滤/删掉, 否则会被合并进角色。

## 属性遮蔽 / FText / 枚举: 写"调 C++ 逻辑"的验收脚本必踩 (2026-09-18, BaoBiaoStreet)

### 1. `bool bFoo` 会在 Python 里遮蔽同名函数 `IsFoo()` ← 最容易误判成"逻辑坏了"

C++ 侧很常见的写法是"属性 + 蓝图纯函数读取器":

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsThreat = true;
UFUNCTION(BlueprintPure) bool IsThreat() const { return bIsThreat; }
```

Python 侧 **UPROPERTY 胜出**:`npc.is_threat` 拿到的是 **bool 值**,
再按"BlueprintPure 要加括号"的惯例写 `npc.is_threat()` 就抛
**`'bool' object is not callable`** —— 报错看起来像逻辑错,其实是绑定层命名冲突。

判据:去头文件的 `UPROPERTY` 列表里搜 `b + 首字母大写的同名属性`;
有就是属性,没有才是方法。写取值 helper 一劳永逸:

```python
def val(obj, name, *args):
    """兼容取值: 同名 UPROPERTY(属性) 与 UFUNCTION(方法) 都可能命中"""
    a = getattr(obj, name, None)
    if a is None:
        return None
    return a(*args) if callable(a) else a
```

### 2. `FText` 参数直接传 Python `str`

`unreal.Text` **没有** `from_string`(实测 AttributeError)。给 `UFUNCTION(FText)` 传参数时
直接塞 Python 字符串即可,绑定层会隐式转换:`director.set_objective("TEST OBJECTIVE")`。

### 3. 枚举 `str()` 出来的是 `<ENUM.NAME: 0>` 不是 `"Name"`

Python 3.11+ 对 IntEnum 的 `str()` 是 `"<BBMNpcState.IDLE: 0>"`(全大写枚举名 + 数值),
所以 `"Idle" in str(state)` 这种断言**恒 False**(会被误读成状态机没生效)。
断言统一写成 `str(state).upper()` 后匹配大写名,或直接用 `state.name`。

⚠️ **枚举名里的下划线是最阴的一刀**(2026-09-18 实踩):`str(state)` 给出的是
`"<BBMNpcState.MOVE_TO: 1>"` —— 名字是 `MOVE_TO` 不是 `MOVETO`。
断言写 `"MOVETO" in s` 会**恒 False**,而报错信息里 `MOVE_TO` 和 `MOVETO` 肉眼几乎分不出来,
极容易被误判成"状态机没生效"而去翻 C++ 源码。
稳妥写法:抽一个 helper 只取名字再比,别用子串包含:

```python
def state_name(actor):
    s = str(actor.get_editor_property("npc_state"))   # '<BBMNpcState.MOVE_TO: 1>'
    return s.split(".")[-1].split(":")[0].strip().upper()   # -> 'MOVE_TO'
```

> 对照:**自己写的 C++ 枚举名如果取 `EState::MoveTo`,Python 侧就是 `MOVE_TO`** ——
> 驼峰一律被拆成大写下划线,别按原样匹配。

## commandlet 下关卡 actor **不跑 BeginPlay** —— 影响的是 C++ 设计,不只是脚本 (2026-09-18)

这是本轮最重要的一条。`-run=pythonscript` 无头跑关卡逻辑时:

> **关卡里已摆放的 actor 只被"加载",不会被 BeginPlay / Tick 驱动。**

后果(排查时极容易误判成"逻辑写错了"):

| 现象 | 真实原因 |
|---|---|
| 事件表 / 航点等"默认数据"读出来是空 | 播种写在 `BeginPlay` 里,根本没执行 |
| 单例式引用(`GActiveDirector`)、跨 Actor 指针是 null | 在 `BeginPlay` 里认领的 |
| 定时器心跳不跑、时间恒为 0 | `SetTimer` 在 `BeginPlay` 里注册 |

**所以这不是"脚本要绕开"，而是 C++ 该改的设计:**

1. **默认数据一律在构造函数里播种**(`SeedDefaultTable()` / `SeedDefaultWaypoints()`)。
   好处是三重的:编辑器里摆下的实例立刻带数据 → 无头可验收 → PIE 行为一致。
   构造函数里 `FText::FromString(TEXT("..."))`、`TArray::Add`、`Reset` 都安全(不依赖 World)。
   `BeginPlay` 里再调一次是幂等的,想兜底就留着。
2. **跨 Actor 引用用"懒查找"而非 `BeginPlay` 认领**:在 getter 里判空后现场
   `UGameplayStatics::GetActorOfClass(...)`。(本项目的 `GetDistanceToLady()` 因此去掉了 `const`。)
3. **纯无参 getter / 状态查询完全可在无头下验收**——这正是把逻辑从图迁到 C++ 的最大收益:
   `Tools/phase17_verify.py` 68 项断言(壳父类 / 事件推进 / 接口分发 / NPC 状态机 /
   触发器驱动 / 门闸 / 大小姐)一次跑完,秒级回归。
4. 反过来接:**只有 `BeginPlay` 之后才存在的东西**(比如无头下没有 `PlayerPawn`)不要硬断言,
   返回 `-1.f` / `nullptr` 这类哨兵值是合理的,脚本里断言"可调用且类型正确"即可。
5. **状态赋值不要关在 `if (AAIController* AI = GetAI())` 里面**(2026-09-18,同轮发现)。
   `if (AI) { AI->MoveTo(...); State = MoveTo; }` 这种写法在**没有 controller 时状态就丢了**
   (无头验收、Possess 尚未发生、controller 掉了都中招),表现为"状态机永远读出 Idle"。
   正确写法是**先落状态(意图),再 `if (AI)` 做执行** —— 执行手段不该决定状态记录。
   这不是为了迁就脚本,而是让状态机在 controller 晚于/缺失时依然能表达意图。
6. **要测"玩家做的动作"可以在编辑器世界现场造一个 pawn**:
   关卡里通常没有预摆的玩家角色(那是 PIE 时由 `DefaultPawnClass` 生成的),
   用 `EditorActorSubsystem.spawn_actor_from_class(unreal.XxxPawn, loc)` 现造一个,
   调它的接口断言,测完 `destroy_actor` 收拾干净(脚本不 save_level 就不会落盘)。
7. **测"距离触发"类逻辑要先检查场景前提**:例:验"打电话引来附近围观者",
   若大小姐在 x≈500 而围观者在 x≈7600,半径 2600 的判定**不触发是正确行为** ——
   得先把大小姐 `set_actor_location` 挪到人群附近,否则是在测一个不成立的场景,
   然后误以为 C++ 写错了。**失败时先问"这个场景本身成立吗"。**
8. **别用"恢复类"接口给测试对象复位**:想排除上一轮残留、把 NPC 复位成 Idle 时,
   直觉会调 `RecoverFromStun()`;但如果该 NPC 的配置是"被电击后散场"
   (`bDisbandOnElectric`),恢复路径会**直接走 Disband 分支**,一"复位"全变 DISBANDED,
   把后面所有断言连锁带崩(实测就是这么连环失败 3 条的)。
   复位要**直接写状态字段**:`actor.set_editor_property("npc_state", unreal.BBMNpcState.IDLE)`。
   通用教训:**恢复/结束类接口往往带业务副作用,测试里不要拿它当 setter 用。**

配套:无头验收脚本的骨架就是
`load_level → get_all_level_actors → 按 label 找实例 → 直接调 UFUNCTION 断言`。
不用起 PIE,不用模拟输入,比 PIE 验收快一个数量级。


---

## 2026-09-19 角色绑骨/重定向批量实战新增（本机 5.8 实测）

- **改 IK Retargeter 的 FK 链设置必须"全新构造"**：`IKRetargetFKChainsOpSettings()` + 逐个 `RetargetFKChainSettings()` → `ctl.set_settings()`。
  改现成 struct 再写回会被**静默丢弃**（回读仍是默认值）→ 表现为"改了 rotation_mode 但烘焙结果一模一样"。
- **重建 FK settings 后必须重新 `auto_map_chains(EXACT, True)`**：否则链映射丢失 → 烘焙出**静止动画**（所有帧同一个姿势）。
- `add_retarget_op()` 收**字符串类型名**（`"/Script/IKRig.IKRetargetAlignPoleVectorOp"`），传类对象 → `NativizeProperty ... StrProperty` TypeError。
- `run_batch_retarget(inputs)` 的 `assets_to_retarget` 要 **AssetData**（`EditorAssetLibrary.find_asset_data(path)`），不是资产对象；产出的 AnimSequence **必须 `save_directory` 显式保存**，否则下次启动就没了。
- **`is_inherited=True` 的组件属性改不了**（`BlueprintService.set_component_property` 一律返回 False）→ 改**子类 CDO 的组件实例**：`unreal.get_default_object(unreal.BlueprintEditorLibrary.generated_class(bp))` → `get_components_by_class(...)` → `set_editor_property(...)` → compile+save → **生成实例读回验证**。`HiddenInGame` 也走这条路。
- **protected / 缺失的属性**（本机实测都读不了）：`SkeletalMesh.ref_skeleton`、`BoneNode.name`、`EdGraph.Nodes`。替代：骨骼表 `AnimationLibrary.get_animation_track_names(anim)`、层级 `find_bone_path_to_root(anim, bone)`、每帧变换 `get_bone_pose_for_time(anim, bone, t, False)`；`IKRigController.get_ref_pose_transform_of_bone(b)` 返回的是**父骨系局部变换**（不是世界空间，别拿它算世界朝向）。
- **关卡 GameMode 覆盖**：`world.get_world_settings().set_editor_property("default_game_mode", gm_class)`（`world.get_editor_property("world_settings")` 不存在），改完 `EditorLoadingAndSavingUtils.save_map(w, path)`。
- **同一项目开着编辑器时，`UnrealEditor-Cmd` 会被单实例挡掉**：现象是**零输出零报错**就退出（日志里只有引擎启动脚本）。要么先关编辑器，要么用组播通道 `ue_pyexec.py` 在运行中的编辑器里执行。
- **命令行抓不了图**：`SceneCapture2D` 的渲染目标会被释放（"渲染目标已被释放"）。要 GUI 编辑器 + 两段式：先 `pyexec` 摆姿势，等 5~8 秒让编辑器 tick 出真实骨骼姿态，再 `pyexec` 抓图。
- **UE 的 Python 是 3.11**：f-string 不能跨行、表达式内不能含反斜杠（`f"{a('x')}"` 这类嵌套引号会 SyntaxError）。
- `unreal.MathLibrary.quat_multiply / quat_rotate_vector` **不存在**（AttributeError），四元数自己实现。
- **`MaterialEditingLibrary.create_material` 在 5.8 不存在**（AttributeError）。建材质走
  `AssetToolsHelpers.get_asset_tools().create_asset(名, 目录, unreal.Material, unreal.MaterialFactoryNew())`；
  表达式仍用 `MaterialEditingLibrary.create_material_expression` +
  `connect_material_property(expr, "", unreal.MaterialProperty.MP_BASE_COLOR)`（输出名传空串），
  usage 用 `set_base_material_usage(m, MATUSAGE_INSTANCED_STATIC_MESHES, True)`（见 GLTF_IMPORT_PITFALLS.md）。
- **`ACharacter` 的网格组件在 SCS 里叫 `Mesh`，不是 `CharacterMesh0`**。`BSVC.list_components()` 返回的是
  SCS 节点名；`cdo.get_editor_property("character_mesh0")` 会直接
  `Failed to find property 'character_mesh0'`。要读/写默认网格，用
  `unreal.get_default_object(BLIB.generated_class(bp)).get_components_by_class(unreal.SkeletalMeshComponent)[0]`。
- **清空对象引用属性：传文本 `"None"`**（UE 序列化空引用的标准形式），
  如 `set_component_property(p, "Mesh", "SkeletalMesh", "None")` —— 不要传 Python `None`。
- **占位网格缩放别硬编码**：引擎 BasicShapes 的尺寸别猜，用 `mesh.get_bounding_box().get_extent()`
  （半尺寸）反算，例：要把胶囊变成 160cm 高/45cm 宽 →
  `sz = 160/(2*ext.z)`、`sxy = 45/(2*ext.x)`。
- **旧脚本的 Rotator 参数序坑**：`unreal.Rotator(pitch, yaw, roll)`。见到
  `Rotator(rot[1], rot[2], rot[0])` 这种"自造映射"要把想要的 yaw 写进 pitch 槽
  （长椅倒扣/路障躺平/LED 趴地的根因）。批量修复规则：生成网格里
  `pitch!=0 && yaw==0` → `set_actor_rotation(Rotator(0, pitch, 0))`。
- **`/Engine/BasicShapes/` 里没有 Capsule**：`load_asset("/Engine/BasicShapes/Capsule.Capsule")`
  返回 None，而 `set_static_mesh(None)` 会把组件**清空**且不报错 —— 表现成"赋值后读回仍是 None"。
  实际只有 Cube / Cylinder / Sphere / Plane（Cylinder 是 100×100×100 的半格）。要用占位胶囊
  就自己建或换 Cylinder；赋值前必须判 `cap is not None`，并回读验证。
- **`StaticMeshComponent` 赋网格必须用 `set_static_mesh(mesh)`**：
  `set_editor_property("static_mesh", mesh)` 会**静默失效**（读回仍 None，不抛异常）。
  判断"赋值成功没有"只能回读，别信返回值。
- **`Box` 结构体在 Python 里是 `min`/`max` 属性，没有 `get_extent()`**。半尺寸 =
  `(abs(bb.max.x-bb.min.x)/2, abs(bb.max.y-bb.min.y)/2, abs(bb.max.z-bb.min.z)/2)`。
- **commandlet(`-NullRHI`) 下 `line_trace_single` 不报错但永远打不中**（物理场景不可用）。
  无头验证"东西在不在地上"要用解析式地面模型（按生成脚本的几何参数推），或者开 GUI 编辑器再 trace。
- **`unreal.TraceTypeQuery` 枚举成员叫 `TraceTypeQuery1`/`TraceTypeQuery2`**（不透明数字，
  1=Visibility, 2=Camera），没有 `VISIBILITY` 这种名字。
- **无 PIE 体检的价值**：phase17/19 这类数据断言验不出"角色沉进地里/占位件悬浮"这种几何问题。
  静态体检该查：角色落点（地面高 vs 胶囊半高）、占位件与胶囊对齐（rel Z + 缩放自洽）、
  路面通行性（包围盒）、NavMesh 覆盖、材质空槽。**ACharacter 的 Actor 原点 = 胶囊中心**
  （在地面以上 `capsule_half_height` 处），所以占位网格 rel Z=0、缩放后高度=2*half_height 才贴地。
- **若 Actor 的根组件就是碰撞体（如自定义 AEventTrigger 的根是 TriggerBox），
  "Actor 位置"与"根组件 RelativeLocation"是同一份数据，绝不能各设一份** —— 各设一份会让
  碰撞盒落在世界 2 倍位置（事件永远踩不到），而 `get_actor_location()` 看起来还是"对的"。
  验证碰撞盒位置必须用 `component.get_world_location()`，不要手算 `loc + rel`。
- **`/Engine/BasicShapes/Cylinder` 的包围盒轴与缩放互换**（实测 5.8：高度跟 XY 缩放走、直径跟 Z 走），
  别拿它做占位体 —— 用 Cube（行为正常）。另外继承外部资产的旋转（FBX 常见 roll=±90）会把
  局部轴转到世界上，让缩放看起来"错乱"：先打印 `get_actor_rotation()`。
- **排查"缩放/位置不对"唯一可靠的办法**：以 `get_actor_bounds()` 的实测值为唯一事实，
  迭代"测量 → 按比例修正 → 再测量"直到收敛，别相信 `get_actor_scale3d()` 的读数。
- **工具坑：bash heredoc 里往 Python 字符串里塞 `\n` 会被写成字面 `/n`**，生成的 .py 直接
  SyntaxError。生成 Python 代码后必须先 `ast.parse()` 校验，再交给引擎执行。
  ⚠️ **补充（2026-09-19 角色项目实测）**：写入
  `set_editor_property("relative_rotation", unreal.Rotator(0.0, 270.0, 0.0))` 后回读得到
  `pitch=270, yaw=0` —— 与上面"第 1 个参数是 pitch"**矛盾**。两处证据冲突 → **别赌位置参数**，
  一律用关键字 `unreal.Rotator(pitch=..., yaw=..., roll=...)`，改完立刻回读。

## 无 GUI 改蓝图组件（2026-09-19 实测，第二轮）

- **组件属性名有两套命名**：`SkeletalMesh` / `AnimationMode` / `AnimationData` / `AttachSocketName` /
  `RelativeLocation` / `RelativeRotation` / `RelativeScale3D` / `StaticMesh` 用**显示名**可以；
  但隐藏标志必须写 **`bVisible` / `bHiddenInGame`**（写 `Visibility`/`HiddenInGame` 一律返回 False，**且不报错**）。
  拿不准先 `BlueprintService.list_component_properties(bp, 组件名)` 看真实属性名。
- **继承组件**（`CharacterMesh0` / `CollisionCylinder`）用 `set_component_property` **全 False**；只能取 CDO：
  `unreal.get_default_object(BlueprintEditorLibrary.generated_class(bp))` → `get_components_by_class(...)`
  → `set_editor_property(...)`，再 `compile_blueprint` + `save_asset`；换新编辑器会话回读**确认已落盘**
  （本项目实测有效）。注意 CDO 的组件列表**只含继承组件**，自己 `add_component` 加的不在其中。
- **`bp.get_editor_property("ubergraph_pages")` / `("simple_construction_script")` 报
  `Failed to find property`**：Python 只反射 EditAnywhere 属性，**拿不到 UEdGraph / SCS**。
  读图一律走 `BlueprintService.get_nodes_in_graph / get_node_details / get_node_pins / get_connections`
  （`get_connections` 只吃 **2 个参数**，传 3 个 TypeError）。签名与示例可直接
  `unreal.BlueprintService.<方法>.__doc__` 打印，比猜快得多。
- **`configure_node(..., "VariableName", "WeiniMesh")` 返回 True 但实际没改**
  （回读 `get_node_details().variable_name` 仍是 `Mesh`）→ 这类 configure 必须回读验证。
- **SCS 添加的组件不是蓝图变量**：`variable_exists(bp,"X")=False`、`discover_nodes(bp,"X")=[]`、
  `create_node_by_key(bp,G,"SPAWN K2Node_VariableGet|获取X")` 返回**空串** → EventGraph 里引用不到它。
  `reparent_component(bp,"子组件","继承组件")` 返回 **False**；`add_component(..., parent_name="CharacterMesh0")`
  返回 True 但父级仍是 root，并弹 ensure
  `Node->ParentComponentOrVariableName.IsNone() || ... != PendingParent->GetVariableName()`。
  **绕法**：能被当父级/能 reparent 的只有"自己 add 出来的组件"。
- **exec 输出引脚只能连一条线，但同一张图里可以有多个 `Event Tick`**：老脚本留下的另一套链路每帧也跑，
  后执行的 `PlayAnimation` 覆盖先执行的 → 症状是"改了没反应 / 永远某个动作"。
  用 `get_nodes_in_graph` 数 Tick 事件数与 `PlayAnimation` 节点数即可判定；治理用清场重建 + 回读自检。
- **中文编辑器 `node_title` 是本地化显示名**（`分支`/`事件Tick`/`获取Mesh`），按英文名匹配会漏。
  判据用 `get_node_details()` 的 **`function_name` / `node_class`**（`BreakVector`/`K2Node_CallFunction`）；
  spawner key 本身也是中文的（`SPAWN K2Node_IfThenElse|分支`）。
- **`Get Mesh` 在 Character 蓝图里 = 继承的 `ACharacter::Mesh`（`CharacterMesh0`）**，**不是**你后加的网格组件。
  状态机 `PlayAnimation(Get Mesh, ...)` 会把动画打在隐藏的模板网格上，可见网格只能回落到自己的
  `AnimationData.AnimToPlay` —— 写死成 Jog 就是"永远在跑步"。
- **`-ExecCmds="py xxx.py"` 在编辑器第 0 帧执行**（地图/子系统未就绪）：用
  `unreal.register_slate_post_tick_callback` 等若干帧再干活；关卡用
  `LevelEditorSubsystem.load_level(...)` 显式加载。
- **启动方式**：Git Bash 里 `cmd /c bat` + `start ""` 在本机**起不来**（打印成功但进程不存在、日志不动），
  直接调 exe 全路径才可靠；编辑器**开着时**再起命令行实例会**零输出零报错**（单实例），先杀进程。
  日志出现 `LogD3D12RHI: Error: GPU crash detected ... PageFault` + `TerminateOnGPUCrash`
  → 进程**退出码 3**，属**偶发显卡侧崩溃**（连续启停多次后遇到过一次），重启编辑器即可，不是资产坏了。
- **PIE**：`LevelEditorSubsystem.editor_request_begin_play()` 是真 Play（`editor_play_simulate()` 不 Possess
  玩家）；该脚本流程里 `HighResShot` **没能落盘**，脚本化截图暂不可用。

## 蓝图编译错误怎么读（别再靠删节点二分）

```python
bp = unreal.EditorAssetLibrary.load_asset(P)
ok = unreal.BlueprintEditorLibrary.compile_blueprint(bp)
print(bp.get_editor_property("status"))          # BlueprintStatus.BS_ERROR / BS_UP_TO_DATE(_WITH_WARNINGS)
ge = unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, "AnimGraph")
print(ge.list_nodes_with_errors())               # ★ 坏节点的完整对象路径，直接点名
```
- `compile_blueprint` 只返回 bool；`status` 只说成败。**门禁两条一起看**：
  `status ∈ {BS_UP_TO_DATE, BS_UP_TO_DATE_WITH_WARNINGS}` **且** `list_nodes_with_errors() == []`。
- ★ **2026-09-22 修正：`error_msg` 那条老写法对 `AnimGraphNode_*` 无效。**
  实测对 `AnimGraphNode_Root` / `AnimGraphNode_BlendSpacePlayer` 读 `error_msg` / `error_type`
  **都报 `Failed to find property 'error_msg'`**（这两个属性是 `K2Node` 侧的，原生 AnimGraphNode 没有）。
  正解是 **`BlueprintGraphEditor.get_graph_editor_by_name(bp,"AnimGraph").list_nodes_with_errors()`** ——
  它直接吐坏节点的**完整对象路径**，例如本次实测：
  ```
  ["/Script/BlueprintGraph.K2Node_VariableGet'/Game/TARecovery_Staging/ABP_Weini_Minimal.ABP_Weini_Minimal:AnimGraph.K2Node_VariableGet_0'"]
  ```
  ⇒ 取路径尾段（`K2Node_VariableGet_0`）就能接 `BS.get_node_details / get_node_pins` 去看它到底是什么节点。
  **这条把"二分删节点找错"彻底干掉了**：看一次列表就知道是谁。
- ⚠️ `find_graph` 返回 **EventGraph 时是 `EdGraph`，没有 `get_graph_nodes_of_class`** → 只有 AnimationGraph 能这么遍历；
  EventGraph 的节点问题用 `BlueprintService.get_node_details()`（`connections` 为空 = 被修剪）来判。
- ⚠️ 编辑器加载资产时会自己编一次，日志里会留**陈旧**的 `编译器` 报错行 → 判据只看脚本里最后一次 compile 与 `list_nodes_with_errors()`。

## AnimGraph / AnimGraphService 本轮实测补充
- **AnimGraph 里不要放 impure Cast**（带 `then/CastFailed` 的 `K2Node_DynamicCast`）：官方要求 AnimGraph 只读 AnimBP 自己的变量。
  正解：**EventGraph** 里 `UpdateAnimation → TryGetPawnOwner → Cast → Get 外部变量 → SelectFloat → Set <AnimBP 自己的变量>`，
  AnimGraph 只 `Get` 那个变量。★ 这就是"ADS 分层 compile=False"的真凶。
- `add_layered_blend(...)` 之后**必须 `BlueprintService.refresh_node(lb)`**，否则 `ADS.Pose → BlendPoses_0` 永远连不上（返回 False）。
- `BlendWeights_0` 是 `TArray<float>` 第 0 项暴露出的**单个 float 引脚**（不是数组引脚：**不要 MakeArray**，也不要喂 double）。
- BranchFilter（从某根骨起叠加）：`AnimGraphNode_LayeredBoneBlend.node` → 设 `blend_mode = BRANCH_FILTER` +
  `layer_setup[0].branch_filters = [BranchFilter(bone_name="spine_02", blend_depth=0)]` → `set_editor_property("node", 结构体)`。
  `unreal.BranchFilter` / `unreal.InputBlendPose` / `unreal.LayeredBoneBlendMode.BRANCH_FILTER` 都存在；
  **该类没有 `reconstruct_node()`**（AttributeError，但结构体写入仍生效）。
- 状态机：`add_state_machine` 返回**字符串**（别当元组解包）；`validate_state_machine` 返回结构体（`is_valid/state_count/transition_count`）；
  `set_state_animation(..., loop=False)` 对 Land/JumpStart 是必须的。
- **读状态机当前状态**：`anim.get_anim_instance().call_method("GetCurrentStateName", args=(0,))`（0 = 第一个 StateMachine，已验证可用）。

### ★★ AnimGraph 里 `Get`「继承变量」会硬失败（2026-09-22 实测，写图最大坑）

**症状**：在一个新建的 AnimBP（父类 `TARecoveryAnimInstance`，父类有 `BlueprintReadOnly` 的 `float Speed`）里，
往 AnimGraph 放一个 `K2Node_VariableGet`（节点标题 `Get Speed`）去喂 BlendSpace 的 X →
`compile_blueprint` → **`BS_ERROR`**，`list_nodes_with_errors()` 点名
`…AnimGraph.K2Node_VariableGet_0`。

**关键**：`configure_node(P,G,id,"VariableName","Speed")` **返回 True 但改不动**（回读仍是坏节点）；
`get_node_details()` 里 `variable_name` 也不给你线索。**这不是 API bug，是引擎语义**：
**AnimGraph 只允许读「AnimBP 自己」的变量**，父类（继承）变量在 AnimGraph 里 `Get` 一律编译失败
（与"AnimGraph 不放 impure Cast"是同一族限制）。

**两条正解**：
1. 走 **EventGraph**：`UpdateAnimation → TryGetPawnOwner → Cast → Get 外部/父类变量 → Set 到 AnimBP 自己的变量`，
   AnimGraph 只 `Get` **自己**那个变量。（要新建自己的变量：`BlueprintEditorLibrary.add_member_variable`。）
2. ★**纯函数链（最省事，本项目 `ABP_Weini2` 用的就是这条）**：**零变量、零 Cast、编译干净**——
   `TryGetPawnOwner.ReturnValue → GetVelocity.self`；
   `GetVelocity.ReturnValue → VectorLength.A`；
   `VectorLength.ReturnValue → BlendSpacePlayer.X`。

### 写图（建节点 / 连线 / 删节点）的标准序列 —— 2026-09-22 实测可用

```python
P, G = '/Game/TARecovery_Staging/ABP_Weini_Minimal', 'AnimGraph'
BS = unreal.BlueprintService

# 1) spawner key 一律"程序化发现"，不要手打中文（错一个全角引号就静默返回空串）
for r in (BS.discover_nodes(P, 'Get Velocity', '', 40) or []):
    print(r.spawner_key, '|', r.node_class, '|', r.display_name)

# 2) 建节点 → 返回 node_id 字符串（空串 = 失败，必须判）
n_vel = BS.create_node_by_key(P, G, key_vel, 60.0, 460.0)

# 3) ★先打一遍引脚名再连（实测：self 引脚叫 self、VectorLength 的输入叫 A 不是 Value）
for p in BS.get_node_pins(P, G, n_vel):
    print(p.pin_name, p.pin_type, 'in' if p.is_input else 'out')

# 4) 连 → 编译 → 回读 → 存盘
BS.connect_nodes(P, G, n_pawn, 'ReturnValue', n_vel, 'self')
unreal.BlueprintEditorLibrary.compile_blueprint(abp)
for c in (BS.get_connections(P, G) or []):
    print(c.source_node_title.replace('\n',' '), '.', c.source_pin_name,
          '->', c.target_node_title.replace('\n',' '), '.', c.target_pin_name)
unreal.EditorAssetLibrary.save_loaded_asset(abp)

# 5) 删错节点（日志会打印 LogTemp: DeleteNode: Deleted node '<id>'）
BS.delete_node(P, G, node_id)
```

- **`BS.get_connections` 的第一个实参吃「资产路径字符串」**（`'/Game/.../ABP_Weini2'`），**不是资产对象**。
- 实测引脚名：目标 self 引脚 = **`self`**；`GetVelocity` 输出 = `ReturnValue`；`VectorLength` 输入 = **`A`**。
- **编译成功判据**：`status` 是 `BS_UP_TO_DATE_WITH_WARNINGS/BS_UP_TO_DATE` **且** `list_nodes_with_errors()==[]`
  （只信 bool 返回值会漏掉警告级问题）。

## 输入（Enhanced Input）在纯 Python 下的硬限制
- **`unreal.InputActionFactory` 不存在** → 建不了全新 `InputAction` 资产；只能 `EditorAssetLibrary.duplicate_asset(已有IA, 新路径)` 复制。
- `SPAWN K2Node_EnhancedInputAction|IA_<名字>` 是**一个 IA 一个 key**（IA 资产存在后 key 才进 discover）。
- **`UInputMappingContext.map_key` 没暴露给 Python**；`IMC.default_key_mappings` 是 `InputMappingContextMappingData`（`.mappings` 可读写），
  但里面 **`FKey` 的 key_name 读不出也写不进**（`unreal.Key()` 不接受参数 → TypeError）。
  → 改键位只剩：**改 `Config/DefaultInput.ini` 的 `+ActionMappings=(ActionName="Aim",...,Key=RightMouseButton)`**（重启编辑器生效），或手工 GUI。
- 蓝图变量要被脚本在运行时设置，需 **Instance Editable**：
  `BlueprintEditorLibrary.set_blueprint_variable_instance_editable(bp, "Aiming", True)`（无返回值，调用即可）。

## 截图 / 看画面（脚本侧）
- ✗ `RenderingLibrary.export_render_target` 写出的 PNG **没有有效像素数据**（Blender: "does not have any image data"）；
  同流程 `HighResShot` 也不落盘 → **场景截图管线本机不可用**。
- ✓ **替代**：PowerShell 全屏抓屏（`System.Windows.Forms`+`System.Drawing` 的 `CopyFromScreen`），
  配 `user32` 的 `EnumWindows/ShowWindow/SetForegroundWindow` 先把干扰浮窗最小化、主窗口最大化再抓。
  ⚠️ **PS 脚本里不要写中文**（Git Bash heredoc → PowerShell 按 GBK 读 → 语法错误）；用 Write 工具落 .ps1。
- ⚠️ **`SkeletalMeshActor` 在 PIE 里可能"存在但完全不渲染"**（世界里有、mesh 有、`is_visible()=True`、坐标对，画面里没有；原因未查明）
  → 展示动画就把动画**播在玩家自己的 Mesh 上**（`PlayAnimation` + `PrintString` 轮播）。

## 无 GUI 批量跑脚本的会话纪律（2026-09-19 实测，避免"卡住"）

- **单实例陷阱**：编辑器开着时再起命令行实例 = 新进程把请求转给已有窗口后**自己退出**，
  表现为「进程秒退 + 无日志 + 无产出 + 轮询永远等不到」。所以：
  **每次跑脚本前先 `Stop-Process UnrealEditor -Force`，脚本结束时 `unreal.SystemLibrary.quit_editor()`。**
  两件事都做，才能连续跑几十轮不出事。
- **在 Bash 工具里后台化 UE 不能用 `nohup ... &`**（父命令一结束子进程被杀）→ 必须用工具的
  `run_in_background=true`；否则同样是「秒退无产出」。
- 轮询要**同时检测进程存活**：进程没了而产物没生成 = 启动失败，立刻停，别空等到超时。
- ⚠️ **`Stop-Process -Force` / `taskkill /F` 不是"干净的关"，它会留下自动保存** →
  下次启动弹「资源恢复包」（用户会被反复打扰）。强杀后**必须顺手隔离** `Saved/Autosaves/` 整个目录
  与 `Saved/*.tmp`（移到 `Saved/Autosave_quarantine_<日期>/`，别删）。完整收尸流程见
  `UE_EDITOR_LIFECYCLE_AND_HYGIENE.md`。
- ⚠️ **不要为了"锦上添花"的只读校验去盲试未知 API**：本文件下面那节其实已经写明了 `get_connections`
  的正确字段名（`source_node_id / source_node_title / source_pin_name / target_node_id / target_node_title /
  target_pin_name`），我却绕过手册去 `getattr` 猜 `from_node_id` 等替代名 → 编辑器 crash（`Saved/Crashes/UECC-*`）。
  **顺序永远是：先翻手册 → 再取一条样本打印字段 → 最后才批量处理**，且探测与批量分两次运行。

## 蓝图取证：读连线 / 读动画引用（正解 API）

- ✗ `Blueprint.ubergraph_pages` 在 **UE5.8 已不存在**（`Failed to find property`）→ 别再用 dump_v5 的老写法。
- ✗ `EdGraph.get_graph_nodes()` 不存在（只有 `AnimGraph` 类图能按类枚举）。
- ✗ **`g.get_editor_property('nodes')` 会「静默」返回空**（2026-09-22 实测，最阴的一条）：
  它**不抛异常**，只是给出空/None → 我用 `safe()` 包装后落盘成 `"AnimGraph": []`，
  差点据此判定"**这个 AnimBP 的 AnimGraph 是空的**"。**实际图是好的**（下面 ✓ 那条能读出 2 节点 + 4 连线）。
  ⇒ 规则：**`AnimGraph.Nodes` 是 protected，"取到空"必须当"读法不对"处理，不许当"没有"**。
- ✓ **读 AnimGraph 节点正解**：`find_graph(bp,'AnimGraph').get_graph_nodes_of_class(unreal.AnimGraphNode_Base, True)`
  → 实测返回 `AnimGraphNode_Root`（标题「输出姿势」）+ `AnimGraphNode_BlendSpacePlayer`（标题含 `BS_Weini_Locomotion`）。
- ✗ **在 PIE 运行中**调 `BS.get_nodes_in_graph(BP,"EventGraph")` 会返回 **0 节点**（假象）；
  编辑器态同一调用返回 43 节假 → **取证一律在编辑器态做**。
- ✓ **读连线正解**：`BS.get_connections('/Game/.../ABP_Weini2', graph_name)`
  （**第一实参是资产路径字符串，不是资产对象**）→ 每项含
  `source_node_id / source_node_title / source_pin_name / target_node_id / target_node_title / target_pin_name`。
  找断链：`BS.get_node_pins(AB, graph, node_id)` 里 pin 的 `is_connected`（`False` 就是断的）。
  常见断点：**`事件BeginPlay` 的 `then` 与下游 `按类添加组件(K2Node_AddComponentByClass)` 的 `execute` 之间**——
  中途插入/清理调试链（如检片轮播）很容易把这条边删掉，症状是「BeginPlay 里加的组件运行时完全不出现」。
  修复：`BS.connect_nodes(AB, graph, beginplay_id, "then", addcomp_id, "execute")` → 回读 `is_connected=True`。
- ✗ `EditorAssetLibrary.does_asset_exist()` 有**假阴性**（磁盘上存在的 uasset 报 False）→ 用 `load_asset()` 实测。
- ✓ **状态机动画回读**：`AGS.get_used_anim_sequences(ABP)` → 每项
  `sequence_name / sequence_path / used_by_node / used_in_graph`，能精确看出「哪个状态在用哪条动画、有没有残留旧动画」。
- `AGS.set_state_animation(ABP, "Locomotion", 状态名, "资产路径.资产名", loop, blend)` 是**替换语义**
  （旧动画会从引用里消失），返回值是**节点 id 字符串**而不是 True/False——**别拿返回值当成功证据**，
  改完必须用 `get_used_anim_sequences` 复验。
- 转换 blend：读 `AGS.get_state_transitions(ABP, "Locomotion", 源状态)`（含 `blend_duration/priority/rule_summary`），
  写 `AGS.set_transition_blend(ABP, "Locomotion", 源, 目标, 时长)`。落地/起身这类转换实测过 0.05~0.08s 会明显发硬，
  调到 0.25~0.30 才顺。

## ★ `pin_type` 只报基类型：FVector 是 `struct`，**没有 `"vector"` 这个值**（2026-09-19 实测，重要）

给引脚做类型过滤时最容易写错的地方。本机（UE 5.8.2 中文版）全图 449 个引脚去重枚举，`pin_type` 只会出现这些：

```
exec / object / bool / real / struct / string / name / delegate / byte / class
```

**没有 `vector`、没有 `float`、没有 `double`、没有 `int`。** 对应关系实测：

| 蓝图里看到的类型 | `pin_type` 实际值 |
|---|---|
| FVector / FVector2D / FTransform 等结构体 | **`struct`** |
| float / double（含 Pin 上的"real"） | `real` |
| int / byte | `byte` |
| bool | `bool` |
| 对象引用 / 组件引用 / self | `object` |
| 执行引脚 | `exec` |

真实踩坑：专家脚本写 `require_pin(node, ("SocketOffset","Socket Offset"), False, False, ("vector",), "GetSocket value")`
→ **必然 0 命中、抛 FATAL**，而节点上明明有一个名叫 `SocketOffset`、`is_input=False` 的引脚：

```
pin lookup failed: GetSocket value on <nodeid>
  SocketOffset  input=False  type=struct  default=0, 0, 0      ← 就是它，但被 ("vector",) 过滤掉
  self          input=True   type=object  default=
```

**纪律**：
- 过滤 FVector 引脚写 `("struct",)`，或干脆**留空 `()` 只按名字匹配**（更稳，因为同名冲突少见）。
- 想确认本机支持哪些 type token，就跑一次"全图引脚类型去重统计"（几十行，可反复用）：
  ```python
  seen = {}
  for nd in BS.get_nodes_in_graph(BP, "EventGraph") or []:
      for p in (BS.get_node_pins(BP, "EventGraph", str(nd.node_id)) or []):
          seen[str(p.pin_type)] = seen.get(str(p.pin_type), 0) + 1
  for k, v in sorted(seen.items(), key=lambda kv: -kv[1]): print(v, k)
  ```
- 同理，**`default_value` 是判断结构体类型的好旁证**：FVector 会打印成 `(x=0.0,y=40.0,z=20.0)` 这样的形式。

## 2026-09-19 资产工厂：`MirrorDataTable` 只能靠官方 GUI 对话框建（程序化路径全线失败，实测穷举）

`UMirrorDataTable`（镜像数据表，动画镜像用；行结构 `FMirrorTableRow` = Name/MirroredName/MirrorEntryType/bEnabled）
在本机 5.8.2 上**没有任何程序化创建路径**。穷举过的死路（共同点：**不报错，只是静默返回 None / 0 行**）：

| 试法 | 结果 |
|---|---|
| `AssetTools.create_asset(名, 目录, unreal.MirrorDataTable, unreal.MirrorDataTableFactory(), ...)` | 返回 **None**，磁盘无资产 |
| 同上但先 `fac.ConfigureProperties()` / 传 preset skeleton / 换 `supported_class` | 同上 |
| `DataTableFactory` + `struct=` 想建同构表 | 同上；`factory=None` 能建出资产但 **RowStruct = None，且 Python 侧只读** |
| CSV / JSON 导入灌行（`AssetImportTask` / `CSVImportSettings`） | 报"导入成功"但 **0 行**；日志 `LogCSVImportFactory: No RowStruct specified` |
| `set_editor_property("row_struct", …)` 硬写 | 只读属性，无效 |

**唯一能建的路**：走官方 Factory 的**模态对话框**（编辑器"新建资产/资产另存为"那条 C++ 路径），脚本发一次：

```python
at.create_asset_with_dialog(MDT_NAME, MDT_DIR, unreal.MirrorDataTable, fac,
                            unreal.Name("BOOTSTRAP"), True)   # 末位 = call_configure_properties
```

**发模态框的脚本纪律**（踩过：对话框"一直在抖 / 根本看不到"，用户点不了）：
- 用 `-ExecCmds="py <脚本文件>"` **直接跑一次**，脚本里**不要写重试循环**——每次重试都再弹/再关一次窗，用户看到的就是"窗口在抖"。
- 对话框可能被别的 UE 顶层窗口盖住：枚举**所有** UE 顶层窗口（`EnumWindows` + 进程名过滤）逐个 `ShowWindow(SW_RESTORE)` + `SetForegroundWindow`，再让用户点。本机标题是本地化名（实测 `资产另存为`）。
- 对话框里要设的三项：**RowStruct = MirrorTableRow、Skeleton = 目标骨架、MirrorAxis**；建完立刻回读校验这三项再往下走。
- 行数据仍可纯脚本读写，所以**"能不能建表"是唯一的 GUI 卡点**，别把它当成"整条路都得手工"。

## 2026-09-19 原生 AnimGraphNode：没有 `node_title` / `pins`，要用官方节点 API 读

按 `n.get_editor_property("node_title")` 取动画图节点标题 → `Failed to find property 'node_title'`（同族还有 `pins`）。
**原生 `AnimGraphNode_*`（SequencePlayer / LayeredBoneBlend / Mirror…）不暴露这两个属性**，能读的是通用 K2Node API：

```python
cls   = str(n.get_class().get_name())      # 'AnimGraphNode_SequencePlayer' / 'AnimGraphNode_LayeredBoneBlend'
title = str(n.get_node_title())            # 官方 K2Node.get_node_title()，本地化显示名
pos   = unreal.BlueprintEditorLibrary.get_node_pos(n)   # 失败时回退 (300,300)，返回 IntPoint
```

- 判类**别用** `n.node_class`（恒 None）；引脚别读 `pins`，用 `BlueprintEditorLibrary.list_input_pins / list_output_pins`
  或 `BlueprintGraphPinLibrary`（`break_single_pin_link(pin, other)` / `try_create_connection(pin, other)` 实测可用）。
- 图编辑器入口：`BlueprintGraphEditor.get_graph_editor_by_name(bp, "AnimGraph")` → `.get_graph()` / `.list_all_nodes()` /
  `.create_node_from_name(动作名, 位置, context_pins)`；`list_available_nodes([])` 列全量动作。

## 2026-09-19 调色板动作名：中文 + 分类前缀，**别拿资产名去过滤**

`list_available_nodes([])` 每项形如 `分类|分类|本地化显示名`（本机中文）：
`动画|杂项|镜像` / `动画|镜像|GetMirrorDataTable` / `动画|镜像|GetMirror` / `工具|结构体|BreakMirror` …
按"动作串里包含 `MDT_<资产名>`"过滤 → **恒为空列表**（资产名根本不在动作名里：表是**建完节点后再赋属性**绑上去的）。

纪律：
- 过滤用**类目 + 稳定英文子串**（`镜像` / `GetMirror` / `MirrorNode` 之类），不要夹资产名/实例名；
- 动作名是本地化的，跨语言要写**多候选回退**，并在建完后**按节点类**（`get_class().get_name()`）校验，而不是看返回字符串；
- 先把 `list_available_nodes` 的原始列表**逐行落盘**再写匹配规则——这份清单就是下次的字典。

## 2026-09-19 ★ `unreal.Rotator` 位置参数构造序是 **(Roll, Pitch, Yaw)**，不是 (P,Y,R)

实测：`unreal.Rotator(11, 22, 33)` 得到 `P=22, Y=33, R=11`。用 `unreal.Rotator(*三元组)` 直接喂"从引脚/文本里读出来的 P,Y,R"会**整支错位**——
真实教训：本该"枪口右转 20°"的一刀变成了"向下掰 22.5°"（用户当场看出来）。

**纪律**：一律 `unreal.Rotator(pitch=..., yaw=..., roll=...)`（关键字）；从文本读出的三元组先拆开再构造。
（注意区分：**引脚/文本里的 Rotator 文本序**在本机是 `Pitch,Yaw,Roll`，见上面 R7.1 那条；两者不是一回事。）

## 2026-09-19 旋转组合与"符号"：交给引擎算、靠测量定案

- 可用：`unreal.MathLibrary.compose_rotators(A,B)`、`get_forward_vector / get_right_vector / get_up_vector(rotator)`、
  `make_rot_from_yz(aim, up)`、`Rotator.quaternion()`；**没有** `rotate_vector_around_axis`。
  让向量绕 Rotator 转 = `v.x*Fwd + v.y*Right + v.z*Up`（用引擎给的基向量）。
- **别自己写 `quat_to_rot`**：自写的 `rot_to_quat` 能和引擎对上，但自写的 `quat_to_rot` 会在 pitch ±90 附近落到另一支
  （往返自检 `(0,0,90) → quat → −90` 即暴露）。要欧拉 → 用引擎的 `compose_rotators` 直接拿 Rotator，别过四元数。
- **合成顺序不能猜**：`compose(delta, base)` 与 `compose(base, delta)` 是"世界/父系"与"物体本地"两种语义，
  实测同一组 delta 用错顺序得到"右 0.87° / 上 −7.92°"而不是"右 14.00° / 上 +8.00°"。
  **做法：两种顺序都算，用引擎读数验证目标量（例如"枪口在自身坐标里右转 14°"），通过的那支才落盘。**
- 顺带一条判定"物体本地哪边是右"的实测法：`make_rot_from_yz(+X,+Z)` = `Rotator(yaw=−90)`，
  它的 X 轴 = 世界 −Y ⇒ 若某模型本地 +Y=枪口、+Z=上，则**本地 +X = 它的左侧**（本地 −X = 右侧）。
  同类问题一律这样量，不靠"UE 是左手系所以……"推。

## 2026-09-19 没有 MCP 时"人在飞、脚本到点再跑"：旗标 + tick 回调 + 姿势锁

- **到点执行**：自己写包装脚本，`register_slate_post_tick_callback` 里轮询一个**旗标文件**；出现就 `exec` 目标脚本
  （读文件用 `encoding="utf-8-sig"` 躲 BOM，见上一条），**执行后删掉旗标** → "再放一次旗标"= 再执行一次，可反复触发。
- **姿势锁**（让人一边自由飞一边保持 ADS 姿势）：每 tick 把 pawn 的 `Aiming` 属性写 `True`
  （很多 BP 角色**没有** `set_aiming` 函数，属性写入才是实测有效通道），并**强制武器网格可见**
  —— 脚本置位不会触发输入事件链，光设 Aiming 武器不显示。
- **别缓存 pawn**：PIE 最初几帧 `get_player_pawn` 给的是 `SpectatorPawn`，缓存它会让后续读数全 NA。
  判定"真角色"要有 SkeletalMesh + 有 SpringArm；取不到再退回缓存/全场景扫描。

## 2026-09-19 ★ 跨工程复制资产：字节相同 ≠ 引用能解析

同一个 `.uasset`（sha256 一致）从工程 A 复制到工程 B 后，`RowStruct`/`MirrorAxis` 都可读，但
`mdt.get_editor_property("skeleton")` 在 A 里 PASS、在 B 里 **None**（而 B 的 ADS 动画能解析到同一个 skeleton）。
→ **不要用"文件哈希一致"推断跨工程引用可用**；要在目标工程里**重新绑定**（`set_editor_property` + `save_loaded_asset`）
并**另起进程 fresh 回读**才算过。

**更要命的后续**：任何之后 save 这个资产的脚本（例如补丁脚本里的"重填表格 + 保存"）**可能把它再冲回 None**。
真实事故：前置门禁读到 skeleton 是好的（PASS），补丁填表+保存后文件**逐字节回到源端那份**，补丁**之后**的 fresh 验证才发现
`MDT Skeleton persisted :: None`。**纪律：凡写这类资产的脚本，"引用字段可解析"的门禁必须同时做在写入之后**（改完立刻另进程复验），
并且优先找"既有的、已验证的资产"复用，不要靠"复制同哈希文件"过关。

## 2026-09-19 小坑：`compact()` 式辅助函数会吃掉下划线，导致"永假"门禁

有个辅助 `compact(s)` 去掉非字母数字（`BP_Handong` → `bphandong`），但匹配式写成
`"bp_handong" in compact(title)` → **永远为假**，于是"图里明明有 BP_Handong"被误判成 FAIL。
同类错误还有：一侧过 `compact`、另一侧拿原始串比。
纪律：**定义一个转换函数后，所有比较对象都要过同一个函数**；字符串门禁失败时，先把**两侧的原始值**都打出来再改匹配逻辑。

## 2026-09-20 ★ 自由相机(Alt+C)轮：5 条实测（"对照参考图定稿相机端点"那一轮）

### 1. `Alt+C` 进自由相机后，`get_player_controller(world,0)` 是 **DebugCameraController**
- 实测：`pc0_class = DebugCameraController`，它的 `get_control_rotation()` = 自由相机自己的旋转。
  ⇒ 任何"自由相机视线 vs 控制旋转/准星方向"的夹角判据都会**自己比自己**，稳定输出 `0.000001°`（假 0）。
- 正确取法：**角色的**控制旋转要走 `character.get_controller().get_control_rotation()`（实测 `BP_ThirdPersonPlayerController_C`）。
- **独立判据**（不依赖 HUD）：自由相机模式下 `PlayerCameraManager.get_camera_location()` **≠** `FollowCamera.get_world_location()`，
  且 `PCM.get_camera_rotation()` **==** `pc0.get_control_rotation()`；两者相等就说明 PCM 还在看生产相机。
- 纪律：凡涉及"控制旋转/准星方向"的判据，**先打印 pc0 的 class 名**再下结论。

### 2. `CameraComponent` 没有 `get_relative_rotation()`（属性读才有）
`camera.get_relative_rotation()` → `AttributeError`；用 `get_editor_property("relative_rotation")`。
**同时**：自己写的旁录脚本必须**逐段 try/except**——一个不存在的绑定抛异常会让整份快照归零
（实测：live JSON 一个字节都没写，而目标脚本因为有兜底照常产出，容易被误判成"旁录没启用"）。

### 3. 弹簧臂的有效相机帧是 **`get_socket_rotation("SpringEndpoint")`**，不是组件自身的世界旋转
`bUsePawnControlRotation=true` 时组件只继承角色朝向（实测：组件旋转 yaw = 角色 yaw `-4.55`，
而 `SpringEndpoint` 旋转 = 控制旋转 `-9.625`——两者不是一回事）。
世界位置 → 可持久化端点的换算（**没有手工正负翻转**）：

```python
origin  = spring.get_world_location() + get_editor_property("target_offset")
Rsocket = spring.get_socket_rotation(unreal.Name("SpringEndpoint"))
local   = unreal.MathLibrary.inverse_transform_location(
              unreal.Transform(location=origin, rotation=Rsocket, scale=unreal.Vector(1,1,1)), desired_loc)
arm = -local.x ;  side = +local.y ;  height = +local.z      # SocketOffset = (0, side, height)
```

符号错 = 参考系错，回去查基底，不要补 flip（补 flip 就是把错误固化进脚本）。

### 4. 按"字面量 + A/B 引脚"定位蓝图节点会**误命中数学节点**
实测拦截：`float * float`（`K2Node_CallFunction`）也有恰好一个 `A`、一个 `B` 引脚，且 `A=0.0 B=0.0` 有输出连接
（`→ float + float.B`），于是"B=0 唯一"的定位判据命中 2 个、门禁正确地 STOP。
- 修法：判据里加**节点类型/标题过滤**（`SelectFloat` 节点的 `get_node_title()` 就是 `"SelectFloat"`），
  并保留"唯一 + 有输出连接"的 STOP；
- 定位成功后**打印它的输出去哪**当证据：实测三个端点分别 `→ FInterpTo.Target`（臂长）、`→ MakeVector.Y`（OffsetY）、`→ MakeVector.Z`（OffsetZ）。

### 5. `compile_blueprint` / `save_asset` 期间会**嵌套泵一次 slate tick** ⇒ 注册的 tick 回调递归重入
实测：同一个 `rev` 在 0.13 秒内被写了 **3 次**；两条历史记录读到的是"刚写进内存的新值"，只有最外层那条读到旧值并最后落盘。
写入幂等所以这次没写坏，但**嵌套那次可能在下一次 rev 之后才落盘 ⇒ 把旧值写回去**。
- 修法：整个状态机跑在 `busy` 标志后面（进入先置位，`finally` 清位，嵌套直接 return）；
- 验收方式：对同一 rev 连发两次，历史里必须**恰好一条**记录。

> 人在环里的完整回路（同帧捕获 / 只重启 PIE 的快循环 / 用户语言契约 / 冻结+真输入验收）见
> `ue-vibecoding/reference/HUMAN_LOOP_TUNING.md`。


---

## 2026-09-20 ★「想当然存在」的 API 又抓出三个 + 单位陷阱（R111→R119 六轮实测）

### 1. `unreal.AutomationScheduler` **根本不存在**（整模块级缺失）

```text
hasattr(unreal,'AutomationScheduler')      -> False
unreal *Automation*                        -> AutomationEditorTask, AutomationLibrary,
                                              AutomationScreenshotOptions, AutomationTestToolset,
                                              AutomationUtilsBlueprintLibrary, AutomationViewSettings ...
unreal *Latent*                            -> 没有 add_latent_command 相关项
存在：AutomationLibrary.take_high_res_screenshot / take_automation_screenshot_at_camera /
      automation_wait_for_loading / finish_loading_before_screenshot
存在：AutomationEditorTask.is_valid_task() / is_task_done()
```
**后果**：脚本里 `@unreal.AutomationScheduler.add_latent_command` 在 **import 期**就 `AttributeError`，
**一行业务都跑不到**（日志里连脚本自己的第一行都没有）。
**正解**：`unreal.register_slate_post_tick_callback(pump)` + **自己写状态机**，并做两件事：
- **重入保护**：`take_high_res_screenshot()` 会再进 Slate ⇒ 无保护会
  `ValueError: generator already executing`（用 `busy` 旗标跳过嵌套调用）；
- 异常必须 `log_error`（否则表现为"挂死 + 无产物"）。

### 2. `StaticMeshComponent` 没有 `b_hidden_in_game`、没有 `get_component_transform`

实测逐字报错：
```text
AttributeError("'StaticMeshComponent' object has no attribute 'b_hidden_in_game'")
AttributeError("'StaticMeshComponent' object has no attribute 'get_component_transform'")
```
可用替代：`is_visible()`、`get_world_location()/get_world_rotation()/get_world_scale()`、
`get_editor_property('relative_location'/'relative_rotation'/'relative_scale3d')`、
`get_attach_parent()`、`get_attach_socket_name()`、`get_editor_property('static_mesh')`。
**★纪律**：只读探针里**每一次读取都各自 try/except 并把失败写进报告的 `failures` 数组**——
否则一处 AttributeError 就让**整块指标变 null**，而"指标缺失"极易被误读成"没问题"。
（R119 实例：`get_component_transform` 一处失败，导致 `gun_relative_to_hand_*` 两个核心对照量**整块没产出**。）

### 3. `APlayerController.WasInputKeyJustPressed(FKey)` 可以用 —— 但**别挂在 0.01s 循环计时器上计数**

用它判"真实物理键有没有到 PlayerController"是有效的（R112 实测 F/LMB/RMB 都能读到）。
但它语义是"上一帧未按、这一帧按下"，**挂高频计时器上读会重复报同一次按下**
（实测：真人点 3 次左键，raw 计数 `29`）。**要计数就用回调/事件计数**，raw 只用来判"有没有到"。

### 4. ★UE 的 world 单位**就是 cm** —— 别再 ×100

```python
mesh.get_socket_location(bone)          # 已是 cm
unreal.SystemLibrary.get_component_bounds(comp)   # -> (origin, extent, sphere_radius)，都是 cm
bounds_size_cm = [2.0*extent.x, 2.0*extent.y, 2.0*extent.z]   # 对
# 错：2*extent.x*100  /  [v*100 for v in loc]
```
乘 100 会把 24 cm 的枪报成 2400 cm，**所有距离门禁失真 100 倍且不报错**。
**自洽交叉验证（建议每次都做）**：`源资产尺寸 × 运行时 world_scale ≈ 运行时 bounds`。
R119 实例：源 GLB 24.05 cm × world_scale 0.82 = 19.72 cm；运行期世界 AABB 20.91×4.81×14.86（旋转盒外接）✓

### 5. 顺带：`get_component_bounds` 的返回值是三元组，不是对象

```python
origin, extent, sphere_radius = unreal.SystemLibrary.get_component_bounds(comp)
```
写成 `bounds.origin` 会 `AttributeError`；且它是**世界轴对齐盒**（旋转物体外接盒偏大），
只能在同配置间做趋势比较，**不能单独当"没有互穿"的证明**。

### 6. `PIE 里等世界就绪`：`get_game_world()` 在 `-ExecCmds` 早期是 `None`

`-ExecCmds="py <script>"` 在编辑器启动早期执行；此时
`get_editor_subsystem(UnrealEditorSubsystem).get_game_world()` 仍是 `None`，
直接快照会 `RuntimeError: No UE world available`。
**正解**：脚本内 `editor_request_begin_play()` → 轮询等 `get_game_world()` 非 None → 再等
`get_players_pawn(world,0)` 非空 →（若需要特定状态）用 §上一节的自校验等待 → 再干活 → `quit_editor()`。


## ABP 读图 / ModifyBone 的实测细节（UE 5.8）

**读 AnimGraph 的可用路径**（本项目实测可用）：
```python
bp = unreal.load_asset('/Game/..../ABP_X')
g  = unreal.BlueprintEditorLibrary.find_graph(bp, 'AnimGraph')      # 返回 AnimationGraph
nds = g.get_graph_nodes_of_class(unreal.AnimGraphNode_Base, True)     # 拿全部节点
for nd in nds:
    inner = nd.get_editor_property('node')      # ← 真正的 AnimNode 数据在这里
```
- ⚠️ **节点属性在 `nd.get_editor_property('node')` 里面**，不在节点自己身上。
- ⚠️ `FStructBase` 的子字段要**再取一层**：`nd.node.get_editor_property('bone_to_modify')`
  再 `.get_editor_property('bone_name')`。直接 `repr()` 一个 struct 会打印成
  `<Struct 'BoneReference' (...) {}>` —— **空壳，看不到任何字段**，非常容易误判为"没设置"。
  实测：`repr` 空壳，但逐层取到的 `bone_name = Name("upperarm_l")`。
- ⚠️ 属性名是**下划线小写**：`bone_to_modify` / `translation_mode` / `rotation_mode` /
  `scale_mode` / `translation_space` / `rotation_space` / `scale_space` / `alpha`。
- ⚠️ `unreal.BlueprintService.get_connections(<资产路径字符串>, 'AnimGraph')`
  —— **要传路径字符串**，传资产对象会 `TypeError`。返回 `BlueprintConnectionInfo` 结构体列表，
  字段：`source_node_title` / `source_pin_name` / `target_node_title` / `target_pin_name`。
- ⚠️ 本机**没有** `unreal.SkeletalMeshLibrary.get_bone_names` /
  `unreal.AnimationLibrary.get_skeleton_bone_names`（都 `AttributeError`）。
  要骨架骨名，退路是直接扫 `*_Skeleton.uasset` 字节里的骨名字符串（本项目 23 根，可靠）。
- ⚠️ 上一轮留下的坑：`list_nodes_with_errors` 等 API 在本机也不存在；
  读节点错误用 `nd.get_editor_property('error_msg')`（AnimGraph 节点有效）。

## 2026-09-26 ★ 绑定名先探后用：本构建"不存在"的 API 清单 + 属性/方法命名坑（Stage B 实机导出轮）

这一轮所有"想当然存在"都失败过一次。**规则**：读引擎状态前先探绑定名，把失败写进报告，
**一个名字写错只花一行日志，不该花掉一次会话**。

### 本构建实测**不存在**（BaoBiaoStreet / UE 5.8.2）

| 想当然 | 现实 |
|---|---|
| `unreal.KismetSystemLibrary.execute_console_command(...)` | `module 'unreal' has no attribute 'KismetSystemLibrary'` |
| `unreal.AutomationBlueprintFunctionLibrary.take_high_res_screenshot(...)` | 同类，模块不存在（该库在 FunctionalTesting 插件里） |
| `unreal.AIBlueprintHelperLibrary.simple_move_to_location(...)` | 本机也不存在（改用 `AIController.move_to_location`） |
| `obj.export_text()`（想 dump 结构体字段表） | `'PlayerController' object has no attribute 'export_text'` —— **别指望它**；改用 `dir(obj)` 过滤 + 逐属性 try/except |

★ 注意：**路由门/hook 里若还写着"结构体读不到就用 `export_text()`"，那是错指令**（本构建没有），已在本轮修正。

### 命名坑（每一条都实测过报错原文）

- **bool 属性丢 `b` 前缀**：`bEnableCameraLag` → `enable_camera_lag`（写 `b_enable_camera_lag` 报
  `Failed to find property`）。
- **protected 属性读不了**：`PC.get_editor_property("pawn")` →
  `Property 'Pawn' for attribute 'pawn' on 'PlayerController' is protected and cannot be read`
  ⇒ 取 pawn 用 `unreal.GameplayStatics.get_player_pawn(world, 0)`（可读）或 `pc.call_method("K2_GetPawn")`。
- **transient 属性根本不暴露**：`predicted_lod_level` → `Failed to find property 'predicted_lod_level'`。
  运行期 LOD 这类量只能靠"别处的证据"（本项目的 A 采集逐帧 `lod.predicted=0`）来 pin，别在 Python 里硬找。
- **`call_method` 的名字不一定等于 C++ 名**：`pc.call_method("GetPawn")` → `Failed to find function 'GetPawn'`，
  而 `pc.call_method("K2_GetPawn")` 成功。同一函数的 Python 属性名（`get_pawn`）可能**两边都不存在**。
- **ScriptName 决定 Python 名**：`USceneComponent::K2_GetComponentToWorld` 的
  `meta=(ScriptName="GetWorldTransform")` ⇒ Python 里是 **`get_world_transform()`**（不是 `k2_get_...`）。
- **out-param 回元组**：`pc.get_viewport_size()` → `(2569, 1348)`，可直接 `list(...)`。

### 探测的写法（照抄）

```python
def try_call(key, fn, void=False):
    """void=True 表示"没抛异常"即成功（把 None 当失败会误报，见下）。"""
    try:
        v = fn()
        if v is None and not void:
            raise ValueError("returned None")
        WORKED[key] = True
        return v
    except Exception as e:
        FAILED[key] = "%s: %s" % (type(e).__name__, e)
        return None

pawn = None
for key, fn in (("GameplayStatics.get_player_pawn", lambda: unreal.GameplayStatics.get_player_pawn(w, 0)),
                ("PC.call_method:K2_GetPawn",       lambda: pc.call_method("K2_GetPawn")),
                ("PC.get_controlled_pawn",          lambda: pc.get_controlled_pawn())):
    pawn = try_call(key, fn)
    if pawn is not None:
        break
```

**void 语义的反面教材**：`editor_request_begin_play()` 返回 `None`；把它当失败 ⇒ 报告写 "FAIL"、
后续什么都没做、白等 120 s。**判据是"没抛异常"。**


## 2026-09-26 ★★`HitResult` 的**三种返回值形态**（两轮实测，比"用 to_dict"更细）

| 情形 | Python 拿到什么 | 怎么判 |
|---|---|---|
| 射线打中 | `HitResult` 结构体，**无字段访问器**（`actor`/`blocking_hit`/`impact_point` 全 absent，`get_editor_property('Actor'/'actor')` 全报 `Failed to find property`） | `d=r.to_dict()`；键 `blocking_hit`/`hit_actor`/`hit_component`/`impact_point`/`location`/`normal`/`distance`/`time`/`trace_start`/`trace_end`/`phys_mat` |
| 射线**打空** | **`None`**（本 CL 实测：`line_trace_single` 返回 None） | `blocking_hit=false` + `miss_signal="binding_returned_none"`，**不是错误** |
| 参数不合法 | 抛 `TypeError`（例：`line_trace_single() required argument 'draw_debug_type' (pos 7) not found`） | 换 arity 重试；`/8` 与 `/7` 可用、`/6` 不可用 |

- `r.export_text()` 在**结构体**上可用（能打印 `bBlockingHit=True, ImpactPoint=(X=…,Y=…,Z=…), Distance=…`），在 **UObject** 上不可用 —— 只能当**诊断**，不要解析它出判定。
- `location`/`impact_point` 是**真 Vector**（`.x/.y/.z` 可读），`to_tuple()` 也可用但按声明顺序、脆。
- 反面教材（真踩过）：`getattr(r, "actor", None)` / `getattr(r, "blocking_hit", False)` —— 在"无字段访问器"的结构体上**永远返回默认值**，把"读不懂"静默降成"没打中"，几何/通道/场景全对也查不出来。

## 2026-10-06 UE 5.8 实测补充（美术版关卡那一天，全部踩过）

- **`UWorld.get_editor_property("streaming_levels")` 不存在**（`Failed to find property 'streaming_levels'`）。
  枚举子关卡用 `unreal.EditorLevelUtils.get_levels(world)` → 每个 `ULevel` 的 **`get_outer()` 才是 ULevelStreaming**
  （把 `get_outer()` 传给 `EditorLevelUtils.move_actors_to_level` 可用；直接传 ULevel 不行）。
- **`EditorLevelUtils.add_level_to_world(...)` + `save_current_level()` 返回 True 也可能没写进文件**：
  实测加完 Demo_01 后 `Main.umap` **体积一字节未变**、文件里 **grep 图名 0 次**、重开后列表里也没有。
  ⇒ 判断"加没加上"必须三看：**文件 sha/体积 + grep 图名 + 重开列 levels**。要真落盘走编辑器 UI
  （Levels 窗口 → Add Existing → Always Loaded → Save）。
- **`EAS.get_all_level_actors()` 返回"编辑器里所有已加载世界"的演员**：如果你之前用 `load_level` 单独开过
  Level1/Level2/Level4，它们也在这个集合里 ⇒ `a.get_outer().get_name()` 全是 `PersistentLevel`，
  按它判"演员属于哪张图"**会全错**。正解：`a.get_package().get_path_name()` /
  `a.get_outer().get_outer().get_path_name()`（得到 `/Game/Maps/<Map>`）。
- **`PrimitiveComponent.bounds` 在 Python 不暴露**（`AttributeError: 'StaticMeshComponent' object has no attribute 'bounds'`）
  → 用 `actor.get_actor_bounds(False)`：返回 `(center, extent)`，**origin 就是中心**，别再 `origin+extent`；
  注意 `get_actor_bounds(True)` 只算"有碰撞的组件"，对全 NoCollision 的美术件会给出无效/空盒。
- **布尔 UPROPERTY 的 Python 名有两套**：`bDebugLog`→`debug_log`（写 `b_debug_log` 会 `Failed to find property`）、
  `bFaceCaptureMode`→`face_capture_mode`、`bGravityRises`→`gravity_rises`。**写之前先 try 候选名，并把命中的名字写进报告。**
- **`unreal.Vector` 没有 `.size()`**（`AttributeError`）→ 用 `math.sqrt(dx*dx+dy*dy+dz*dz)`。
- PIE 读状态：`pawn.get_current_gravity_direction()`（字符串如 `<GSGravityDirection.NEGATIVE_Z: 5>`）；
  方块 `b.can_change_gravity()` / `b.toggle_gravity_z()` / `b.get_gravity_axis_world()`；
  `GameplayStatics.get_all_actors_of_class(w, unreal.GSBlockBase)` 在 PIE 世界可用。
- **编辑器被模态弹窗卡住时（帧 0、脚本不执行）**：先 `Get-Process UnrealEditor | % MainWindowTitle` 看标题；
  需要看全部窗口就用 `user32.dll` 的 `EnumWindows`（"恢复包"/"缺失项目设置！"就是这么认出来的）；
  想看"用户现在看到什么"用 `GetWindowRect`+`PrintWindow` 整窗存 PNG。

- **按标签取演员必须先定关卡**：不同关卡常有同名标签（当天 `SM_Wall_11_V5` 在 Level2 与 Level5 各有一个）。
  取件一律用 `pick(label, prefer_pkg)`（`a.get_package().get_path_name().endswith("Level2.Level2")`）。
- **`LevelEditorSubsystem.load_level()` 不会保存当前持久层**：切图前有未保存的流送/关卡改动就会丢
  ⇒ "add 流送关卡" 必须紧跟 `EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/Main")`（见 ART_ONLY_LEVEL_REPAIR §9）。
- 常用且实测可用的读法：`LevelEditorSubsystem.is_in_play_in_editor()`；`UnrealEditorSubsystem.get_game_world()`；
  `EditorLoadingAndSavingUtils.get_dirty_map_packages()`；组件级 `get_collision_response_to_channel(unreal.CollisionChannel.ECC_PHYSICS_BODY)`；
  `actor.get_actor_bounds(False)`（返回 `(center, extent)`；**平面网格 extent.z 可能是 0**，别拿它当厚度）。
- `spawn_actor_from_class()` 落在**当前关卡**（不是"你正在看的"或"某流送关卡"）：
  要让新演员进 Level2 就先 `set_current_level_by_name("Level2")` 并等 1~1.5s，再用 `a.get_package().get_path_name()` 复验。
- `AGSFunctionalCollisionProxy`：`functional_role` 用 `unreal.GSFunctionalCollisionRole.FLOOR`；
  `configure_as_floor_from_world_bounds(center, extent, 12.0)` 只对**水平地板**成立（会清零旋转 + 压成水平薄板）。
