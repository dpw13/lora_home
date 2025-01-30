# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

set(SHIELD seeed_b2b_sx1262)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(lora_relay)

FILE(GLOB app_sources src/*.c)
FILE(GLOB common_sources ../common/src/*.c)
FILE(GLOB common_board_sources ../common/src/esp32s3/*.c)
target_sources(app PRIVATE ${app_sources} ${common_sources} ${common_board_sources})
target_include_directories(app PRIVATE ../common/src)

target_sources(app PRIVATE
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/hal/hmac_hal.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/hal/aes_hal.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/src/efuse_controller/keys/with_key_purposes/esp_efuse_api_key.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/src/esp_efuse_api.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/src/esp_efuse_utility.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/${CONFIG_SOC_SERIES}/esp_efuse_utility.c
        ${ZEPHYR_HAL_ESPRESSIF_MODULE_DIR}/components/efuse/${CONFIG_SOC_SERIES}/esp_efuse_table.c
)

# The EUI code pokes lorawan internals
set(ZEPHYR_CURRENT_LIBRARY loramac-node)
FILE(GLOB lm_sources ../common/lorawan/*.c)
zephyr_library_sources(${lm_sources})
zephyr_include_directories(${ZEPHYR_BASE}/subsys/lorawan)
