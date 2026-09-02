# 编辑器内 Python API 坑速查

> 什么时候读这个文件:`get_editor_property` 抛异常、类加载返回 None、枚举传参报错、脚本行为和预期不符。

## 属性不存在(改用方法/别的对象)

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
- `move_input` 字段**不反射**,只能设不能读;Vector2D 用 `unreal.Vector2D(x, y)`(Y=前)
- 验证方向:相机前向投影到支撑面 → `dot(velocity, fwd)` 正=W 正确;`up.cross(fwd)` 为右,A 应得负点积

## 存盘

- `EditorAssetLibrary.save_asset` 对某些改动返回 False(假拒,值其实只在内存)→ 改用**静态工具类** `unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)`(注意它是工具类**不是 Subsystem**,`get_editor_subsystem` 会报 "must be a Class");存完用磁盘 mtime 确认真的落盘
