set(requires esp_wifi nvs_flash bt esp_driver_rmt esp-tls esp_netif esp_http_server fatfs esp_event vfs spiffs)
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
else()
    list(APPEND requires esp_wifi esp_eth)
endif()

idf_component_register(SRCS "led_strip_encoder.c" "RmtTxDriver.cpp" "mount.c" "file_server.c" "wifi_station.c" "Ledstrip.cpp" "sntp.c" "webserver.cpp" "main.cpp"
                    PRIV_REQUIRES ${requires}
                    INCLUDE_DIRS ".")
spiffs_create_partition_image(storage "../website" FLASH_IN_PROJECT)