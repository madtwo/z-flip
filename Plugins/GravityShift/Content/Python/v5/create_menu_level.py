# -*- coding: utf-8 -*-
"""创建主菜单关卡 /Game/MainMenu,并把它的 GameMode 覆盖设成 AGSMenuGameMode。

为什么需要这个脚本:菜单关卡的 GameMode 是**逐关卡**覆盖(WorldSettings.DefaultGameMode),
不写进任何 ini,所以没法靠配置文件搞定 —— 必须有一个能被序列化进去的关卡资产。
.uasset/.umap 是二进制,代码生成不了,只能让编辑器建。

用法(编辑器已启动且加载的是本项目):
    python ue_pyexec.py "exec(open(r'<此文件绝对路径>', encoding='utf-8').read())"

幂等:关卡已存在时直接打开并重设 GameMode,不会新建第二个。
"""
import unreal

LEVEL_PATH = "/Game/MainMenu"
GAME_MODE_CLASS = "/Script/GravityShift.GSMenuGameMode"

level_ss = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
editor_ss = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)

print("[menu] current level before: " + editor_ss.get_editor_world().get_path_name())

if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
    print("[menu] level exists, opening it")
    print("[menu] load_level: " + str(level_ss.load_level(LEVEL_PATH)))
else:
    print("[menu] new_level: " + str(level_ss.new_level(LEVEL_PATH)))

print("[menu] current level now: " + editor_ss.get_editor_world().get_path_name())

# WorldSettings 里存 GameMode 覆盖。**必须走 world.get_world_settings()**:
# 空关卡的 WorldSettings 是懒创建的,不出现在 get_all_level_actors() 里;
# 之前用 spawn_actor_from_class 兜底造了个野的,PIE 会把它当"多余的 World Settings"
# 销毁掉,覆盖随之丢失 —— 症状是菜单关卡里照样生成滚球 Pawn。
world_settings = editor_ss.get_editor_world().get_world_settings()
print("[menu] world settings: " + str(world_settings))

game_mode_class = unreal.load_class(None, GAME_MODE_CLASS)
print("[menu] game mode class: " + str(game_mode_class))

world_settings.set_editor_property("default_game_mode", game_mode_class)
print("[menu] override now: " + str(world_settings.get_editor_property("default_game_mode")))

print("[menu] saved: " + str(level_ss.save_current_level()))
print("[menu] DONE")
