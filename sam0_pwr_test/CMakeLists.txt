# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(sam0_pwr_test)

add_subdirectory(../common lora-common)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources} ${common_sources} ${common_board_sources})
