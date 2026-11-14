#!/usr/bin/env python3
"""Apply a small, checked Emscripten adaptation to the pinned KUI simulator."""
from pathlib import Path
import sys

sdk = Path(sys.argv[1])
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
    "${CMAKE_CURRENT_LIST_DIR}/../../../sdk/components/kode_sys/include)",
    "KGUI component header include path",
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
