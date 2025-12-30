#pragma once

#include "driver/rmt_tx.h"

#define RMT_LED_STRIP_GPIO_NUM      CONFIG_LED_STRIP_GPIO_NUM
#define EXAMPLE_LED_NUMBERS         CONFIG_LED_NUMBERS

typedef enum {
    ALGO_MONO,
    ALGO_RAINBOW
} ledstrip_algo_t;

class Ledstrip {
    rmt_channel_handle_t led_chan;
    rmt_tx_channel_config_t tx_chan_config; 
    rmt_encoder_handle_t led_encoder;
    rmt_transmit_config_t tx_config;
    uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];
    uint32_t loopcnt;

public:
    ledstrip_algo_t algorithm;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t bright;
    uint32_t speed;
    uint32_t startled;

    Ledstrip();
    void loop();

    esp_err_t init();
    void monocolor();
    void rainbow();
    void switchLeds();
};
