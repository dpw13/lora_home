# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(SHIELD seeed_b2b_sx1262)

# It seems appropriate to put this *after* the Zephyr package but it only
# affects the build if we put it here. I'm surely doing something wrong.
set_property(GLOBAL APPEND PROPERTY extra_post_build_commands
        COMMAND ${PYTHON_EXECUTABLE} "$ENV{ZEPHYR_BASE}/../common/scripts/incr_build.py"
        ARGS ${CMAKE_CURRENT_SOURCE_DIR}/VERSION
)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lora_garage)

add_subdirectory(../common lora-common)

FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources} ${common_sources} ${common_board_sources})

target_sources(app PRIVATE
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/hal/hmac_hal.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/hal/aes_hal.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/src/esp_efuse_api.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/src/esp_efuse_utility.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/${CONFIG_SOC_SERIES}/esp_efuse_utility.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/${CONFIG_SOC_SERIES}/esp_efuse_table.c
)
