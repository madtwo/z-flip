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

## 历史案例速查

- **v2 六向 G/R 修复**(2026-09-01):BindKey 对关卡实例失效 → 改 Tick 轮询,重编译 14s,OS 注入验收 G=横移 2156cm、R=RESET 归位 ✅
- **v5 滚球翻转验收**(2026-09-02):toggle ACCEPTED→球升空(广播唤醒)→摄像机 slerp→撞棚顶 FALL_THRESHOLD 自动反向→reset 语义 NO_CHANGE 正确 ✅。遗留:用户实机 WASD/G/E/R 游玩验收
- **金币案例**(PIE 实测 ✅):传送玩家观察吸附/销毁的验收法即出自此,详见 BLUEPRINT_EDITOR.md
