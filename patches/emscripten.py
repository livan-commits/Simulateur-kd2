#!/usr/bin/env python3
"""Apply a small, checked Emscripten adaptation to the pinned KUI simulator."""
from pathlib import Path
import shutil
import sys

sdk = Path(sys.argv[1])
project = Path(__file__).resolve().parents[1]

# Use the same upstream QP/C 8.1.4 POSIX host port as the native simulator.
# The ESP-IDF port is not usable in a browser build because it requires FreeRTOS.
port_source = project / "ports/qpc-posix"
port_target = sdk / "components/qpc/port"
port_target.mkdir(parents=True, exist_ok=True)
for name in ("qp_port.h", "qp_config.h", "qf_port.c", "safe_std.h"):
    source = port_source / name
    if not source.is_file():
        raise SystemExit(f"Missing vendored QP/C POSIX port file: {source}")
    shutil.copy2(source, port_target / name)
(port_target / "sdkconfig.h").write_text(
    "/* Browser host configuration; not an ESP-IDF board configuration. */\n"
    "#ifndef SIMULATEUR_KD_SDKCONFIG_H\n"
    "#define SIMULATEUR_KD_SDKCONFIG_H\n"
    "#define CONFIG_KODE_SCHEDULER 0\n"
    "#define CONFIG_KODE_TEST_DOT 0\n"
    "#endif\n"
)
cmake_path = sdk / "components/kui/sim/CMakeLists.txt"
main_path = sdk / "components/kui/sim/main.c"

def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return source.replace(old, new, 1)

cmake = cmake_path.read_text()
cmake = replace_once(
    cmake,
    "find_package(PkgConfig REQUIRED)\npkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)",
    "if(NOT EMSCRIPTEN)\n"
    "    find_package(PkgConfig REQUIRED)\n"
    "    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)\n"
    "endif()",
    "SDL2 package discovery",
)
cmake = replace_once(
    cmake,
    "    else()\n"
    "        target_link_libraries(${target} PRIVATE PkgConfig::SDL2 m)\n"
    "    endif()",
    "    elseif(EMSCRIPTEN)\n"
    "        target_compile_options(${target} PRIVATE \"-sUSE_SDL=2\")\n"
    "        target_link_options(${target} PRIVATE\n"
    "            \"-sUSE_SDL=2\"\n"
    "            \"-sALLOW_MEMORY_GROWTH=1\"\n"
    "            \"-sEXIT_RUNTIME=0\"\n"
    "            \"-sASSERTIONS=1\")\n"
    "        target_link_libraries(${target} PRIVATE m)\n"
    "    else()\n"
    "        target_link_libraries(${target} PRIVATE PkgConfig::SDL2 m)\n"
    "    endif()",
    "Emscripten SDL2 target options",
)
cmake_path.write_text(cmake)


# The upstream KGUI simulator compiles screen sources directly rather than
# registering the KGUI ESP-IDF component. Add the component root so headers
# such as kgui_edge.h resolve in this standalone CMake build.
consumer_cmake_path = sdk.parent / "components/kgui/sim/CMakeLists.txt"
consumer_cmake = consumer_cmake_path.read_text()
consumer_cmake = replace_once(
    consumer_cmake,
    "INCLUDES ${CMAKE_CURRENT_LIST_DIR}/../screens)",
    "INCLUDES ${CMAKE_CURRENT_LIST_DIR}/../screens "
    "${CMAKE_CURRENT_LIST_DIR}/.. "
    "${CMAKE_CURRENT_LIST_DIR}/../include "
    "${CMAKE_CURRENT_LIST_DIR}/../glyphs "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kode_sys/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kdefs/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/klog/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kdrv/apps/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kaos/core/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kqp/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/qpc/qpc/include "
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/qpc/port)",
    "KGUI component header include path",
)
consumer_cmake_path.write_text(consumer_cmake)

# The pinned SDK keeps KHAL and KDRV public headers in per-service folders.
# Mirror the native host patch so transitive public headers (notably
# kqp_signals.h -> khal_sync.h) resolve in the browser build as well.
host_include_dirs = []
for component in ("khal", "kdrv"):
    host_include_dirs.extend(
        path for path in sorted((sdk / "components" / component).glob("*/include"))
        if path.is_dir()
    )
include_anchor = "    \${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/qpc/port)"
if consumer_cmake.count(include_anchor) != 1:
    raise SystemExit("KGUI host include path: expected exactly one QP/C port anchor")
include_paths = "\n".join("    " + str(path) for path in host_include_dirs)
consumer_cmake = consumer_cmake.replace(
    include_anchor,
    "    \${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/qpc/port\n"
    + include_paths + ")",
    1,
)
consumer_cmake_path.write_text(consumer_cmake)

main = main_path.read_text()
main = replace_once(
    main,
    '#include <string.h>\n',
    '#include <string.h>\n\n#ifdef __EMSCRIPTEN__\n#include <emscripten.h>\n#endif\n',
    "Emscripten header",
)
main = replace_once(
    main,
    "static void pump(uint32_t ms)\n{\n"
    "    for ( uint32_t t = 0U; t < ms; t += 5U )\n"
    "    {\n"
    "        (void)lv_timer_handler();\n"
    "        SDL_Delay(5U);\n"
    "    }\n"
    "}\n",
    "static void pump(uint32_t ms)\n{\n"
    "    for ( uint32_t t = 0U; t < ms; t += 5U )\n"
    "    {\n"
    "        (void)lv_timer_handler();\n"
    "        SDL_Delay(5U);\n"
    "    }\n"
    "}\n\n"
    "#ifdef __EMSCRIPTEN__\n"
    "static void browser_loop(void)\n"
    "{\n"
    "    (void)lv_timer_handler();\n"
    "}\n"
    "#endif\n",
    "browser event-loop callback",
)
main = replace_once(
    main,
    "    sim_opts_t o;\n"
    "    if ( parse(argc, argv, &o) != 0 )",
    "    sim_opts_t o;\n"
    "#ifdef __EMSCRIPTEN__\n"
    "    char *browser_argv[] = { (char *)\"kgui_sim\", (char *)\"face\", (char *)\"--device\" };\n"
    "    if ( argc == 1 )\n"
    "    {\n"
    "        argv = browser_argv;\n"
    "        argc = 3;\n"
    "    }\n"
    "#endif\n"
    "    if ( parse(argc, argv, &o) != 0 )",
    "browser default screen",
)
main = replace_once(
    main,
    "    pump(400U);\n",
    "#ifndef __EMSCRIPTEN__\n"
    "    pump(400U);\n"
    "#else\n"
    "    (void)lv_timer_handler();\n"
    "#endif\n",
    "browser initial render",
)
main = replace_once(
    main,
    "    for ( ;; )\n"
    "    {\n"
    "        pump(5U);\n"
    "    }\n"
    "}",
    "#ifdef __EMSCRIPTEN__\n"
    "    emscripten_set_main_loop(browser_loop, 0, 1);\n"
    "    return 0;\n"
    "#else\n"
    "    for ( ;; )\n"
    "    {\n"
    "        pump(5U);\n"
    "    }\n"
    "#endif\n"
    "}",
    "browser main loop",
)
main_path.write_text(main)
print("Emscripten adaptation applied to the pinned KUI simulator.")


# The QP/C ESP-IDF port includes sdkconfig.h, which is generated by idf.py
# in firmware builds. The browser simulator is a host build, so provide an
# intentionally empty configuration header: no ESP-IDF-only AO watch options
# are enabled in this standalone simulator.
config_header = sdk / "components/qpc/port/sdkconfig.h"
config_header.write_text(
    "#ifndef KODE_SIM_SDKCONFIG_H\n"
    "#define KODE_SIM_SDKCONFIG_H\n"
    "/* Host/Emscripten simulator: no ESP-IDF Kconfig options. */\n"
    "#endif\n"
)
