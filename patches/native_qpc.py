#!/usr/bin/env python3
"""Prepare the pinned KGUI simulator for a native host build."""
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 3:
    raise SystemExit("usage: native_qpc.py <kode-sdk-dir> <project-root>")

sdk = Path(sys.argv[1]).resolve()
root = Path(sys.argv[2]).resolve()
source = root / "ports/qpc-posix"
target = sdk / "components/qpc/port"
target.mkdir(parents=True, exist_ok=True)

# QP/C 8.1.4 POSIX port from the matching upstream release. This modifies
# only the isolated checkout used by the native simulator build.
for name in ("qp_port.h", "qp_config.h", "qf_port.c", "safe_std.h"):
    src = source / name
    if not src.is_file():
        raise SystemExit(f"Missing vendored official POSIX port file: {src}")
    shutil.copy2(src, target / name)

# kqp_config.h includes sdkconfig.h for two optional scheduler/test features.
# The host build deliberately disables both optional firmware features; this
# is an explicit simulator configuration, not a fabricated ESP-IDF header.
(target / "sdkconfig.h").write_text(
    "/* Generated host configuration; not an ESP-IDF board configuration. */\n"
    "#ifndef SIMULATEUR_KD_SDKCONFIG_H\n"
    "#define SIMULATEUR_KD_SDKCONFIG_H\n"
    "#define CONFIG_KODE_SCHEDULER 0\n"
    "#define CONFIG_KODE_TEST_DOT 0\n"
    "#endif\n"
)

# The upstream KGUI standalone target compiles screen sources directly.
consumer_cmake = sdk.parent / "components/kgui/sim/CMakeLists.txt"
cmake = consumer_cmake.read_text()
old = "INCLUDES ${CMAKE_CURRENT_LIST_DIR}/../screens)"
new = (
    "INCLUDES ${CMAKE_CURRENT_LIST_DIR}/../screens "
    "${CMAKE_CURRENT_LIST_DIR}/.. "
    "${CMAKE_CURRENT_LIST_DIR}/../include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kode_sys/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kdefs/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/klog/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kdrv/apps/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kaos/core/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kqp/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/qpc/qpc/include "
) + str(target)
# KHAL and KDRV are split into per-service/per-chip include directories.
# Add real upstream headers; do not replace hardware APIs with stubs.
for component in ("khal", "kdrv"):
    component_dirs = sorted((sdk / "components" / component).glob("*/include"))
    for include_dir in component_dirs:
        if include_dir.is_dir():
            new += " " + str(include_dir)
new += ")"
if cmake.count(old) != 1:
    raise SystemExit(
        "KGUI include path patch: expected exactly one upstream match, "
        f"found {cmake.count(old)}"
    )
consumer_cmake.write_text(cmake.replace(old, new, 1))

print("Native KGUI include paths and official QP/C 8.1.4 POSIX port prepared.")
