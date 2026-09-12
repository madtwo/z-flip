# -*- coding: utf-8 -*-
"""UGSSettingsSaveGame 的读写 + 钳位自检。

用法(编辑器已启动且加载本项目):
    python ue_pyexec.py "exec(open(r'<此文件绝对路径>', encoding='utf-8').read())"

会写真实存档槽 ZFlipSettings,跑完把值恢复成 1.0。
"""

import unreal

assert hasattr(unreal, "GSSettingsSaveGame"), "GSSettingsSaveGame 没注册:模块没编译?"

Save = unreal.GSSettingsSaveGame


def read_back():
    return Save.load_or_create().get_editor_property("mouse_sensitivity_multiplier")


failures = []


def check(label, got, want):
    ok = abs(got - want) < 1e-4
    print("{} {}: got {} want {}".format("PASS" if ok else "FAIL", label, got, want))
    if not ok:
        failures.append(label)


# 1. 写进去再读回来(跨 LoadGameFromSlot,不是内存里的同一个对象)
Save.save_multiplier(2.5)
check("roundtrip", read_back(), 2.5)

# 2. 越界钳位 —— 手改存档文件或蓝图乱传值时,相机不能反向/失控
Save.save_multiplier(-5.0)
check("clamp low", read_back(), 0.1)

Save.save_multiplier(99.0)
check("clamp high", read_back(), 3.0)

# 3. 收尾:恢复默认,别把测试值留给玩家
Save.save_multiplier(1.0)
check("restore", read_back(), 1.0)

print("RESULT: " + ("ALL PASS" if not failures else "FAILED " + str(failures)))
