# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lora_ctrl)

FILE(GLOB app_sources src/*.c)
FILE(GLOB common_sources ../common/src/*.c)
FILE(GLOB common_board_sources ../common/src/atsamd21/*.c)
target_sources(app PRIVATE ${app_sources} ${common_sources} ${common_board_sources})
target_include_directories(app PRIVATE ../common/src)

# The EUI code pokes lorawan internals
set(ZEPHYR_CURRENT_LIBRARY loramac-node)
FILE(GLOB lm_sources ../common/lorawan/*.c)
zephyr_library_sources(${lm_sources})
zephyr_include_directories(${ZEPHYR_BASE}/subsys/lorawan)
