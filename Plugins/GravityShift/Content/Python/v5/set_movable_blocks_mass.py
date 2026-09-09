# GravityShift v5 - batch: raise MassOverrideKg of every movable non-breaker block in the OPEN level.
# Deliverable #2 of the "ball cannot push movable blocks (mass plan)".
# Selection rule (matches the C++ meaning of "pushable by the ball"):
#   bStartSimulatingPhysics && bAffectedByGravity && !bCanBreakTargets
# i.e. blocks that simulate and move under GS gravity, but are NOT smash-capable breakers
# (breakers keep MassOverrideKg=90 - their smash energy 0.5*m*v^2 scales with mass).
# Requires the target level to be open in the editor. Run via editor python console / MCP.

import unreal

TARGET_MASS_KG = 5000.0


def set_movable_block_mass():
    cls = unreal.find_class("AGSBlockBase")
    if cls is None:
        unreal.log_warning("[GravityShift] AGSBlockBase class not found")
        return

    actors = unreal.EditorLevelLibrary.get_all_level_actors()
    count = 0
    for a in actors:
        if not isinstance(a, cls):
            continue
        if not (a.get_editor_property("bStartSimulatingPhysics")
                and a.get_editor_property("bAffectedByGravity")
                and not a.get_editor_property("bCanBreakTargets")):
            continue

        a.set_editor_property("mass_override_kg", TARGET_MASS_KG)
        # Re-route config so Mesh->SetMassOverrideInKg / GravityBody stay in sync
        # (actor without a BlockProfile relies on this at BeginPlay too).
        a.apply_current_configuration()
        count += 1

    unreal.EditorLevelLibrary.save_current_level()
    unreal.log("[GravityShift] set movable-block mass=%s on %d actor(s)" % (TARGET_MASS_KG, count))


set_movable_block_mass()
