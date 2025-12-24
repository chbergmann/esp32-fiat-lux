set(requires esp_wifi nvs_flash bt esp_driver_rmt esp-tls nvs_flash esp_netif esp_http_server)
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs protocol_examples_common)
else()
    list(APPEND requires esp_wifi esp_eth)
endif()

idf_component_register(SRCS "wifi_station.c" "led_strip_example_main.c" "led_strip_encoder.c" "sntp.c" "webserver.c"
                    PRIV_REQUIRES ${requires}
                    INCLUDE_DIRS ".")
