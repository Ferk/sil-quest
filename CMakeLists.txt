# File: CMakeLists.txt
#
# CMake build configuration for Sil-Q.
#
# Supported frontends:
#   - Terminal: GCU (curses/ncurses) on Unix-like systems
#   - Web:      Emscripten/WebAssembly
#
# Terminal build:
#   cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Web build:
#   emcmake cmake -G Ninja -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release -DBUILD_ENABLE_WERROR=ON
#   cmake --build build-wasm
#
# Debug terminal build with sanitizers:
#   cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_ENABLE_WERROR=ON -DBUILD_ENABLE_SANITIZERS=ON
#   cmake --build build
#

cmake_minimum_required(VERSION 3.16)
message(STATUS "CMake version: ${CMAKE_VERSION}")

# `-DCMAKE_EXPORT_COMPILE_COMMANDS` generates `build/compile_commands.json`,
# which is needed for tools like clang-tidy and the clangd LSP server
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Restrict the available build configurations to only "Debug" and "Release" for
# multi-configuration generators (like Ninja Multi-Config, Xcode, Visual Studio)
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)

# =============================================================================
# Set build defaults
# =============================================================================

# During early configure, the command-line toolchain file is already known, but
# CMAKE_SYSTEM_NAME and the EMSCRIPTEN convenience variable are not reliable yet.
set(SIL_USING_EMSCRIPTEN_TOOLCHAIN OFF)
if ((DEFINED CMAKE_TOOLCHAIN_FILE)
    AND (CMAKE_TOOLCHAIN_FILE MATCHES "([/\\\\]|^)Emscripten\\.cmake$"))
    set(SIL_USING_EMSCRIPTEN_TOOLCHAIN ON)
elseif ((CMAKE_SYSTEM_NAME STREQUAL "Emscripten")
    OR (DEFINED EMSCRIPTEN AND EMSCRIPTEN))
    set(SIL_USING_EMSCRIPTEN_TOOLCHAIN ON)
endif()

if (WIN32 AND NOT SIL_USING_EMSCRIPTEN_TOOLCHAIN)
    message(FATAL_ERROR
        "Native Windows frontend support has been removed. "
        "Build the terminal frontend on a Unix-like system or the web frontend with Emscripten.")
endif()

if (SIL_USING_EMSCRIPTEN_TOOLCHAIN)
    option(SUPPORT_GCU_FRONTEND "Build with GCU (curses/ncurses) support" OFF)
    option(SUPPORT_WEB_FRONTEND "Build with Web (Emscripten) support" ON)
elseif (UNIX)
    option(SUPPORT_GCU_FRONTEND "Build with GCU (curses/ncurses) support" ON)
    option(SUPPORT_WEB_FRONTEND "Build with Web (Emscripten) support" OFF)
else()
    option(SUPPORT_GCU_FRONTEND "Build with GCU (curses/ncurses) support" OFF)
    option(SUPPORT_WEB_FRONTEND "Build with Web (Emscripten) support" OFF)
    message(FATAL_ERROR
        "Only Unix-like terminal builds and Emscripten web builds are supported.")
endif()

if (NOT SUPPORT_GCU_FRONTEND AND NOT SUPPORT_WEB_FRONTEND)
    message(FATAL_ERROR
        "At least one frontend must be enabled: SUPPORT_GCU_FRONTEND or SUPPORT_WEB_FRONTEND.")
endif()

if (SUPPORT_WEB_FRONTEND AND NOT SIL_USING_EMSCRIPTEN_TOOLCHAIN)
    message(FATAL_ERROR
        "SUPPORT_WEB_FRONTEND requires an Emscripten toolchain.")
endif()

if (SIL_USING_EMSCRIPTEN_TOOLCHAIN AND SUPPORT_GCU_FRONTEND)
    message(FATAL_ERROR
        "The GCU terminal frontend is not supported for Emscripten builds.")
endif()

if (SUPPORT_GCU_FRONTEND)
    message(STATUS "GCU (curses/ncurses) terminal frontend enabled")
endif()
if (SUPPORT_WEB_FRONTEND)
    message(STATUS "Web (Emscripten) frontend enabled")
endif()

# =============================================================================
# Project settings
# =============================================================================

# Version information defined here is injected into `src/defines.h`.
project(Sil
    VERSION 1.5.1.0
    LANGUAGES C
    DESCRIPTION "Sil-Q is a free, single-player dungeon infiltration game")
set(PROJECT_VERSION_SUFFIX "-beta1")
set(PROJECT_VERSION_STRING "${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}")

message(STATUS "Project version: ${PROJECT_VERSION} (${PROJECT_VERSION_STRING})")

# =============================================================================
# Project metadata
# =============================================================================

set(SIL_EXECUTABLE "sil")

# =============================================================================
# Host environment
# =============================================================================

# Detect host operating system
set(OS_NAME "unknown")
if (APPLE)
    set(OS_NAME "macos")
elseif (WIN32)
    set(OS_NAME "windows")
elseif (UNIX AND NOT APPLE)
    set(OS_NAME "unix")
endif()
message(STATUS "Host operating system: ${OS_NAME}")

# Detect host architecture
set(OS_ARCH "amd64")
string(REGEX MATCH "(arm64|aarch64)" IS_ARM "${CMAKE_SYSTEM_PROCESSOR}")
if (IS_ARM)
    set(OS_ARCH "arm64")
endif()
message(STATUS "Host architecture: ${OS_ARCH}")

# =============================================================================
# Source files
# =============================================================================

# Low-level utility modules
set(Z_SOURCES
    src/z-form.c
    src/z-rand.c
    src/z-term.c
    src/z-util.c
    src/z-virt.c
)

# Core game logic
set(SIL_SOURCES
    src/birth.c
    src/cave.c
    src/cmd1.c
    src/cmd2.c
    src/cmd3.c
    src/cmd4.c
    src/cmd5.c
    src/cmd6.c
    src/adjacent-actions.c
    src/current-square.c
    src/dungeon.c
    src/files.c
    src/generate.c
    src/init1.c
    src/init2.c
    src/load.c
    src/melee1.c
    src/melee2.c
    src/monster1.c
    src/monster2.c
    src/obj-info.c
    src/object1.c
    src/object2.c
    src/randart.c
    src/save.c
    src/spells1.c
    src/spells2.c
    src/squelch.c
    src/tables.c
    src/ui-item-select.c
    src/ui-inventory.c
    src/ui-map-marks.c
    src/ui-model.c
    src/use-obj.c
    src/util.c
    src/variable.c
    src/travel.c
    src/wizard1.c
    src/wizard2.c
    src/xtra1.c
    src/xtra2.c
)

# Base sources used by all platforms
set(BASE_SOURCES ${SIL_SOURCES} ${Z_SOURCES})

set(PLATFORM_SOURCES src/main.c)

if (SUPPORT_WEB_FRONTEND)
    list(APPEND PLATFORM_SOURCES src/main-web.c)
endif()

if (SUPPORT_GCU_FRONTEND)
    list(APPEND PLATFORM_SOURCES src/main-gcu.c)
endif()

# =============================================================================
# Target definition
# =============================================================================

set(SIL_TARGET "${SIL_EXECUTABLE}")
add_executable(${SIL_TARGET} ${BASE_SOURCES} ${PLATFORM_SOURCES})

target_include_directories(${SIL_TARGET} PRIVATE src)

# =============================================================================
# Compiler settings
# =============================================================================

# C standard: GNU99
if (NOT CMAKE_C_STANDARD)
    set(CMAKE_C_STANDARD 99)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    message(STATUS "Setting C standard to C${CMAKE_C_STANDARD}")
    set(CMAKE_C_EXTENSIONS ON)
    message(STATUS "Enabling compiler-specific C extensions")
endif()

# Common defines
target_compile_definitions(${SIL_TARGET} PRIVATE
    VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    VERSION_MINOR=${PROJECT_VERSION_MINOR}
    VERSION_PATCH=${PROJECT_VERSION_PATCH}
    VERSION_EXTRA=${PROJECT_VERSION_TWEAK}
    VERSION_STRING="${PROJECT_VERSION_STRING}"
)

if (NOT WIN32)
    target_compile_definitions(${SIL_TARGET} PRIVATE HAVE_MKSTEMP)
endif()

# Compiler warnings used by all platforms
target_compile_options(${SIL_TARGET} PRIVATE -Wall -Wextra)

# Extended compiler warnings for terminal/web builds
target_compile_options(${SIL_TARGET} PRIVATE
    -Warray-bounds
    -Wbad-function-cast
    -Wcast-align
    -Wdouble-promotion
    -Wfloat-equal
    -Wimplicit-fallthrough
    -Winline
    -Wmissing-declarations
    -Wmissing-format-attribute
    -Wmissing-include-dirs
    -Wnested-externs
    -Wpointer-arith
    -Wredundant-decls
    -Wswitch-enum
    -Wundef
    -Wunreachable-code
)
# GCC-only compiler warnings (not supported by Clang)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${SIL_TARGET} PRIVATE
        -Wold-style-declaration
        -Wold-style-definition
    )
endif()

# Whether to treat compiler warnings as errors
# Default to ON for Emscripten so local web builds catch the same issues CI catches.
option(BUILD_ENABLE_WERROR "Treat compiler warnings as errors" ${SIL_USING_EMSCRIPTEN_TOOLCHAIN})
if (BUILD_ENABLE_WERROR)
    target_compile_options(${SIL_TARGET} PRIVATE -Werror)
endif()

# Whether to enable ASan/UBSan sanitizers
option(BUILD_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if (BUILD_ENABLE_SANITIZERS)
    target_compile_options(${SIL_TARGET} PRIVATE -fsanitize=address -fsanitize=undefined)
    target_link_options(${SIL_TARGET} PRIVATE -fsanitize=address -fsanitize=undefined)
endif()

# =============================================================================
# Platform-specific configuration
# =============================================================================

if (SUPPORT_WEB_FRONTEND)
    target_compile_definitions(${SIL_TARGET} PRIVATE USE_WEB)

    set(WEB_ROOT_DIR "${CMAKE_SOURCE_DIR}/web" CACHE PATH
        "Directory containing web frontend files")
    set(WEB_LIB_DIR "${WEB_ROOT_DIR}/lib" CACHE PATH
        "Directory for generated web artifacts (sil.js/sil.wasm/sil.data)")
    set(WEB_TILESET_SOURCE "${CMAKE_SOURCE_DIR}/lib/xtra/graf/16x16_microchasm.png")
    set(WEB_TILESET_TARGET "${WEB_LIB_DIR}/16x16_microchasm.png")

    file(MAKE_DIRECTORY "${WEB_LIB_DIR}")

    set_target_properties(${SIL_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${WEB_LIB_DIR}"
    )

    target_link_options(${SIL_TARGET} PRIVATE
        "-sASYNCIFY"
        "-sFORCE_FILESYSTEM=1"
        "-sEXPORTED_RUNTIME_METHODS=['FS','HEAPU8']"
        "-lidbfs.js"
        "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/lib@/lib"
    )

    add_custom_command(TARGET ${SIL_TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${WEB_LIB_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WEB_TILESET_SOURCE}"
            "${WEB_TILESET_TARGET}"
        COMMENT "Syncing web runtime assets to ${WEB_LIB_DIR}"
    )
endif()

if (SUPPORT_GCU_FRONTEND)
    find_package(Curses REQUIRED)
    target_compile_definitions(${SIL_TARGET} PRIVATE USE_GCU)
    target_link_libraries(${SIL_TARGET} PRIVATE ${CURSES_LIBRARIES})
    target_include_directories(${SIL_TARGET} PRIVATE ${CURSES_INCLUDE_DIRS})
endif()

# =============================================================================
# Install
# =============================================================================

install(TARGETS ${SIL_TARGET} RUNTIME DESTINATION .)
