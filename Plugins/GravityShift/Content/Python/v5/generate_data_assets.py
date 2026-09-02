"""GravityShift v5 - create profile DataAssets."""

import json
import os

import unreal

ROOT = "/Game/GravityShift/Data"
report = {"created": [], "failed": [], "fail": False}


def ensure_folder(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def make_asset(folder, name, cls):
    path = "%s/%s/%s" % (ROOT, folder, name)
    ensure_folder("%s/%s" % (ROOT, folder))
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, "%s/%s" % (ROOT, folder), None, factory)
    if asset:
        unreal.EditorAssetLibrary.save_asset(path)
    return asset


def fill(asset, **props):
    for key, value in props.items():
        try:
            asset.set_editor_property(key, value)
        except Exception as exc:
            report["failed"].append("%s.%s: %s" % (asset.get_name(), key, exc))
            report["fail"] = True
    unreal.EditorAssetLibrary.save_asset(asset.get_path_name())
    report["created"].append(asset.get_path_name())


def main():
    gravity = make_asset("Profiles", "DA_GS_Gravity_Default", unreal.GSGravityProfile)
    fill(gravity, profile_id="GRAVITY_DEFAULT", gravity_acceleration_cm=1600.0,
         manual_cooldown_seconds=0.25, automatic_cooldown_seconds=0.75)

    ball = make_asset("Profiles", "DA_GS_Ball_Default", unreal.GSBallProfile)
    fill(ball, profile_id="BALL_DEFAULT", radius_cm=50.0, mass_kg=30.0,
         roll_torque_acceleration=26.0, maximum_planar_speed_cm=1600.0,
         camera_flip_duration_seconds=0.35, camera_arm_length_cm=700.0,
         auto_reverse_fall_speed_cm=1400.0, auto_reverse_fall_distance_cm=2200.0,
         landing_auto_reverse_at_speed_cm=2000.0, bounce_speed_cm=250.0)

    slow = make_asset("Profiles", "DA_GS_Surface_Slow", unreal.GSSurfaceProfile)
    fill(slow, spec=unreal.GSSurfaceModifierSpec(
        profile_id="SLOW", priority=10, gravity_scale_multiplier=0.85,
        maximum_speed_multiplier=0.45, impact_energy_multiplier=0.7,
        gravity_axis_drag_hz=1.5, tangent_drag_hz=1.2))

    fast = make_asset("Profiles", "DA_GS_Surface_Fast", unreal.GSSurfaceProfile)
    fill(fast, spec=unreal.GSSurfaceModifierSpec(
        profile_id="FAST", priority=20, gravity_scale_multiplier=1.35,
        maximum_speed_multiplier=1.8, impact_energy_multiplier=1.6,
        gravity_axis_drag_hz=0.0, tangent_drag_hz=0.05))

    landing = make_asset("Profiles", "DA_GS_Landing_Normal", unreal.GSLandingProfile)
    fill(landing, spec=unreal.GSLandingModifierSpec(
        profile_id="LANDING_NORMAL", priority=10, no_response_below_impact_speed_cm=150.0,
        bounce_speed_cm=250.0, auto_reverse_at_speed_cm=1400.0, bounce_tangential_retention=0.85))

    suppress = make_asset("Profiles", "DA_GS_Landing_Suppress", unreal.GSLandingProfile)
    fill(suppress, spec=unreal.GSLandingModifierSpec(
        profile_id="LANDING_SUPPRESS", priority=20, suppress_response=True))

    break_default = make_asset("Profiles", "DA_GS_Break_Default", unreal.GSBreakProfile)
    fill(break_default, profile_id="BREAK_DEFAULT", breakable=True,
         minimum_impact_energy_j=120.0, maximum_health=100.0, damage_scale_per_j=1.0)

    break_fragile = make_asset("Profiles", "DA_GS_Break_Fragile", unreal.GSBreakProfile)
    fill(break_fragile, profile_id="BREAK_FRAGILE", breakable=True,
         minimum_impact_energy_j=40.0, maximum_health=40.0, damage_scale_per_j=2.0,
         required_source_tag="GravityBreaker")

    block_fixed = make_asset("Profiles", "DA_GS_Block_Fixed", unreal.GSBlockProfile)
    fill(block_fixed, profile_id="BLOCK_FIXED", start_simulating_physics=False,
         affected_by_gravity=False, mass_override_kg=0.0)

    block_gravity = make_asset("Profiles", "DA_GS_Block_Gravity", unreal.GSBlockProfile)
    fill(block_gravity, profile_id="BLOCK_GRAVITY", start_simulating_physics=True,
         affected_by_gravity=True, gravity_scale=1.0, mass_override_kg=40.0,
         maximum_speed_cm=3000.0)

    block_breaker = make_asset("Profiles", "DA_GS_Block_GravityBreaker", unreal.GSBlockProfile)
    fill(block_breaker, profile_id="BLOCK_GRAVITY_BREAKER", start_simulating_physics=True,
         affected_by_gravity=True, can_break_targets=True, use_continuous_collision_detection=True,
         mass_override_kg=90.0, impact_energy_multiplier=2.5,
         impact_source_tag="GravityBreaker")

    collectible = make_asset("Profiles", "DA_GS_Collectible_Default", unreal.GSCollectibleProfile)
    fill(collectible, profile_id="COLLECTIBLE_DEFAULT", value=1.0,
         persist_through_reset=True, required_for_goal=True)

    out_dir = os.path.join(unreal.Paths.project_saved_dir(), "GravityShift")
    os.makedirs(out_dir, exist_ok=True)
    with open(os.path.join(out_dir, "GENERATE_DATA_ASSETS.json"), "w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=1, ensure_ascii=False)

    unreal.log("[GravityShift] generate_data_assets assets=%d fail=%s" % (len(report["created"]), report["fail"]))


main()
