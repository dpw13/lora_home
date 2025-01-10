# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lora_ctrl)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

# The EUI code pokes lorawan internals
set(ZEPHYR_CURRENT_LIBRARY loramac-node)
FILE(GLOB lm_sources lorawan/*.c)
zephyr_library_sources(${lm_sources})
zephyr_include_directories(${ZEPHYR_BASE}/subsys/lorawan)
