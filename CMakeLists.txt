# God of War: Betrayal for the PS Vita.
#
#   cmake -B build -G Ninja ^
#     -DCMAKE_TOOLCHAIN_FILE=C:/vitasdk/share/vita.toolchain.cmake ^
#     -DCMAKE_BUILD_TYPE=Release
#   cmake --build build
#
# Produces build/gow_betrayal.vpk. The game data is not part of it: see
# vita/README_VITA.md for the bank preparation step.
cmake_minimum_required(VERSION 3.16)
project(gow_betrayal LANGUAGES CXX)

if(NOT BUILD_VITA)
  message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=${VITASDK}/share/vita.toolchain.cmake")
endif()

include("$ENV{VITASDK}/share/vita.cmake")
find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# The translated game code relies on the two's-complement wrap the PSP build used.
add_compile_options(-fwrapv -fno-strict-aliasing)

# --- generated sources ------------------------------------------------------
# Game.cpp is specialized for the handheld layout (640x360 + Russian/Ukrainian),
# exactly as the PSP build does it.
set(GAME_VITA_CPP ${CMAKE_CURRENT_BINARY_DIR}/GameVita.cpp)
add_custom_command(
  OUTPUT ${GAME_VITA_CPP}
  COMMAND ${Python3_EXECUTABLE} ${ROOT}/tools/gow_psp_patches.py
          ${ROOT}/src/gen/Game.cpp ${GAME_VITA_CPP}
  DEPENDS ${ROOT}/src/gen/Game.cpp ${ROOT}/tools/gow_psp_patches.py
  COMMENT "Patching Game.cpp for the Vita layout")

# The RU/UK string tables compile into locale_psp.cpp from the shared TSVs.
set(LOCALE_INC ${CMAKE_CURRENT_BINARY_DIR}/locale_strings.inc)
add_custom_command(
  OUTPUT ${LOCALE_INC}
  COMMAND ${Python3_EXECUTABLE} ${ROOT}/psp/gen_locales.py
          ${ROOT}/psp/locales.tsv ${ROOT}/psp/controls_psp.tsv ${LOCALE_INC}
  DEPENDS ${ROOT}/psp/locales.tsv ${ROOT}/psp/controls_psp.tsv ${ROOT}/psp/gen_locales.py
  COMMENT "Generating RU/UK locale tables")

file(GLOB CORE_SRC CONFIGURE_DEPENDS ${ROOT}/src/*.cpp)
file(GLOB GEN_SRC CONFIGURE_DEPENDS ${ROOT}/src/gen/*.cpp)
list(REMOVE_ITEM GEN_SRC ${ROOT}/src/gen/Game.cpp)

# --- executable -------------------------------------------------------------
add_executable(gow_betrayal
  ${ROOT}/vita/main_vita.cpp
  ${ROOT}/vita/audio_vita.cpp
  ${ROOT}/vita/intro_vita.cpp
  ${ROOT}/vita/vita_video.cpp
  ${CORE_SRC}
  ${GEN_SRC}
  ${GAME_VITA_CPP}
  ${ROOT}/psp/locale_psp.cpp
  ${ROOT}/third_party/stb/stb_image_impl.cpp
  ${LOCALE_INC})

target_include_directories(gow_betrayal PRIVATE
  ${ROOT}/src
  ${ROOT}/third_party/stb
  ${ROOT}/psp                 # locale_psp.h, timgm.h
  ${CMAKE_CURRENT_BINARY_DIR} # locale_strings.inc
  ${ROOT}/vita)

target_link_libraries(gow_betrayal
  vita2d
  SceAppMgr_stub
  SceAudio_stub
  SceCommonDialog_stub
  SceCtrl_stub
  SceDisplay_stub
  SceGxm_stub
  SceKernelThreadMgr_stub
  SceLibKernel_stub
  SceProcessmgr_stub
  SceSysmem_stub
  SceSysmodule_stub
  z
  m)

# --- packaging --------------------------------------------------------------
vita_create_self(gow_betrayal.self gow_betrayal)

set(LIVEAREA ${ROOT}/vita/sce_sys)
vita_create_vpk(gow_betrayal.vpk GOWB00001 gow_betrayal.self
  VERSION 01.00
  NAME "God of War: Betrayal"
  FILE ${LIVEAREA}/icon0.png sce_sys/icon0.png
  FILE ${LIVEAREA}/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE ${LIVEAREA}/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE ${LIVEAREA}/livearea/contents/template.xml sce_sys/livearea/contents/template.xml)
