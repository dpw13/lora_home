# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

# It seems appropriate to put this *after* the Zephyr package but it only
# affects the build if we put it here. I'm surely doing something wrong.
set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
        COMMAND ${PYTHON_EXECUTABLE} "$ENV{ZEPHYR_BASE}/../common/scripts/incr_build.py"
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lora_ctrl)

add_subdirectory(../common lora-common)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources} ${common_sources} ${common_board_sources})
