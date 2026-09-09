# GravityShift v5 - one-shot: DA_GS_Block_Gravity.MassOverrideKg 40 -> 5000.
# Deliverable #1 of the "ball cannot push movable blocks (mass plan)".
# Run inside the UE editor (MCP: `python ue.py py @...` / Python console / pythonscript).
# Asset-targeted: only touches this one profile; leaves every other profile intact.
# (Seed already synced in generate_data_assets.py, which would rewrite ALL profiles - use this
#  targeted script instead so any manual editor tweaks to other profiles are not clobbered.)

import unreal

PATH = "/Game/GravityShift/Data/Profiles/DA_GS_Block_Gravity"
TARGET_MASS_KG = 5000.0

try:
    asset = unreal.load_asset(PATH)
    if asset is None:
        print("ASSET_NONE")
    else:
        print("BEFORE=%s" % asset.get_editor_property("mass_override_kg"))
        asset.set_editor_property("mass_override_kg", TARGET_MASS_KG)
        unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=True)
        print("AFTER=%s" % asset.get_editor_property("mass_override_kg"))
except Exception as exc:
    print("EXC: %r" % (exc,))
