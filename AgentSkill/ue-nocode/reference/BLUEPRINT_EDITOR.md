# 蓝图编辑(中文编辑器)工作手册

> 什么时候读这个文件:要建蓝图、连线、找节点 id、蓝图报错、做 SCS 组件。

## 中文编辑器七大坑(实测血泪)

1. **节点 ID 已本地化**:文档里的英文 id(`Transformation|GetActorLocation`)不存在,实际是 `变换|GetActorLocation`;函数名也可能中文化(`游戏|从类生成Actor` = SpawnActorfromClass)。**任何节点先 `find_node_types(graph, type_id_filter, context_pins=[])` 查确切 id**,别信英文文档。注意 `context_pins` 是必填(传 `[]` 即可)。
2. **create_node 模糊匹配**:`乘` 被解析成 `Seconds*FrameRate`,`小于（<）` 被解析成 `Timespan<Timespan`。**创建后必须 get_node_infos 校验 type_id,不符就 delete_node 重试**。
3. **引脚名时有时无**:get_node_infos 返回 `output_pins`/`input_pins`(不是 "pins"),name 字段有时为 None(编辑器异步刷新)。**兜底:手动引脚索引表**(引脚顺序即 index_id,实测 100% 可靠),见 `reference/build_coin.py` 的 PIN 表。
4. **可提升运算符**:`工具|运算符|乘`、`小于（<）` 凭空创建类型随机。**先连线上下文再重定型**:创建后把目标类型的输出连到 A 引脚,节点自动变成对应类型(实测 double → `数学|浮点数|float<float`),连完再校验。
5. **write_graph_dsl 在中文编辑器不可用**:内部硬编码英文 `AddEvent|事件名` 直接断言失败。**改用全手动**:`add_event`(幂等)+ `create_node`(必须带 pos)+ `connect_pins` + `set_pin_value`(value 是字符串)。
6. **SpawnActorfromClass 返回通用 Actor**:目标引脚需要具体类型(如 StaticMeshActor)时插 `工具|Casting|CastToStaticMeshActor` 转换节点,否则连接报 "incompatible types"。
7. **运行时生成的网格在编辑器不可见**:BeginPlay 里 AddComponent/SpawnActor 只在 PIE 生效。**验收必须走 PIE**(见 PIE_TESTING.md)。

## 事件与连线的两个致命坑

1. **add_event 建的是"自定义事件"**(节点类型 AddEvent|Custom|EventTick)——自定义事件**永远不会被引擎 Tick/BeginPlay 回调触发**!真事件覆盖节点要用 create_node + 菜单节点 id:`添加事件|事件Tick`(类型 K2Node_Event,输出带 DeltaSeconds 才是真的)。
2. **connect_pins 对执行引脚是"替换"语义,不是追加**:把同一执行输出连到第二个分支会顶掉第一根线(金币案例:加吸附分支顶掉了销毁分支)。**多分支链式串联**:IsValid→分支1(拾取);分支1.else→分支2(吸附);分支2.else→……数据输出引脚可多路复用,执行输出不行。

## 标准建图流程(带状态恢复)

1. `find_node_types` 查全部节点 id
2. `add_event` 加事件;`create_node` 建节点(**必须带 pos**,坐标唯一、按执行链排布)
3. **按位置找回节点**:`find_nodes(graph, title:"")` → `get_node_infos` → `position` 映射到 key(脚本崩了也不丢节点,重跑认领即可)
4. `connect_pins` / `set_pin_value` 按手动索引表接线赋值
5. `compile_blueprint`:返回 `{"returnValue":null}` = 成功
6. `save_assets` 保存资产;改了关卡要存关卡包
7. 中途失败重跑安全:节点认领幂等,多余节点按位置外删除

## SCS 组件配方(让蓝图里的网格"编辑器也可见")

```python
import unreal
bp = '/Game/ThirdPerson/Blueprints/BP_Coin'   # 一律传字符串路径,不是资产对象
unreal.BlueprintService.add_component(bp, 'StaticMeshComponent', 'CoinMesh')
unreal.BlueprintService.set_component_property(bp, 'CoinMesh', 'StaticMesh', '/Engine/BasicShapes/Cylinder.Cylinder')
unreal.BlueprintService.set_component_property(bp, 'CoinMesh', 'RelativeScale3D', '(X=1.0,Y=1.0,Z=0.15)')
unreal.BlueprintService.set_component_property(bp, 'CoinMesh', 'RelativeRotation', '(Pitch=0.0,Yaw=0.0,Roll=90.0)')
unreal.BlueprintService.set_collision_settings(bp, 'CoinMesh', 'NoCollision', 'WorldDynamic', 'NoCollision', {})  # object_type 必填
unreal.BlueprintEditorLibrary.compile_blueprint(unreal.load_asset(bp))
unreal.EditorAssetLibrary.save_asset(bp)
```
- 属性值一律文本格式(`(X=1.0,...)`);碰撞在 BodyInstance 里,`set_component_property` 设不了,必须走 `set_collision_settings`
- **大坑:新加的 SCS 组件不会出现在已放置的旧实例上**——删掉旧实例重新 `add_to_scene_from_class`(remove_from_scene 已验证可用)
- 实例级变换:`comp.set_relative_scale3d(Vector(...))`、`comp.set_relative_rotation(Rotator(roll,pitch,yaw), sweep, teleport)`(读用 `get_editor_property`)

## 安装器/生成器脚本规则(防污染)

- **异常时绝不创建 fallback 资产**——上一轮安装器因父类判定异常,静默建了 14 个 fallback 垃圾蓝图,事后手动清理
- 每个资产操作单独 try/except,**失败时 log + 跳过**
- 已有资产先 `get_blueprint_parent_class()` 校验父类,匹配则复用、不匹配则保留原资产另走他路,**绝不 delete_asset 契约路径**
- 生成的资产打元数据标签(GS_Origin/GS_PackageVersion/GS_ContractAsset/GS_ParentClass),旧资产归档到 `_LegacyBackup` 而非删除
- 参考实现:`D:/UE/z-flip/Plugins/GravityShift/Content/Python/v5/install_blueprints.py`(验证过 14 BP 零污染)

## 金币案例(已验收,含 PIE 实测)

`BP_Coin` = SCS 金色圆柱(CoinMesh)+ 真 Tick 事件旋转(140°/s)+ 吸附(300cm 内 VInterpTo 吸向玩家)+ 拾取(130cm 内销毁)。金材质 `/Game/ThirdPerson/Materials/M_CoinGold`。完整节点表/引脚索引/连线清单见 `reference/build_coin.py`;修类型转换见 `reference/fix_cast.py`。
PIE 验收法(传送玩家观察吸附/销毁)见 PIE_TESTING.md。

## 原生 C++ 承载逻辑 + BP 壳的架构(v3 起的交付模式)

- BP 测试块蓝图编辑器**空白是设计使然**:原生 C++ 类承载全部逻辑(GSGravityBlock 等),BP 只是资产路径壳
- 改行为 → 改 C++(`Plugins/*/Source/.../Private/*.cpp`);改外观/参数 → 改壳的 Class Defaults(细节面板可见 bSimulatePhysics 等)
- 查蓝图父类用 `get_blueprint_parent_class()`,**别用** `generated_class().get_super_class()`(本环境 AttributeError,见 PYTHON_API_PITFALLS.md)
