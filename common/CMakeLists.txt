
FILE(GLOB common_sources src/*.c)
zephyr_library_sources(${common_sources})

zephyr_library_sources_ifdef(CONFIG_SOC_ESP32S3 src/esp32s3/keys.c)
zephyr_library_sources_ifdef(CONFIG_SOC_SERIES_SAMD21 src/atsamd21/keys.c)

zephyr_include_directories(src)

# The EUI code pokes lorawan internals
set(ZEPHYR_CURRENT_LIBRARY loramac-node)
zephyr_library_sources(../common/lorawan/eui.c)
zephyr_library_sources_ifdef(CONFIG_HAS_PSA_STORAGE_SE ../common/lorawan/se.c)
zephyr_include_directories(${ZEPHYR_BASE}/subsys/lorawan)
