#pragma once

#include "driver/rmt_tx.h"

#define RMT_LED_STRIP_GPIO_NUM      CONFIG_LED_STRIP_GPIO_NUM
#define EXAMPLE_LED_NUMBERS         CONFIG_LED_NUMBERS

class Ledstrip {
    rmt_channel_handle_t led_chan;
    rmt_tx_channel_config_t tx_chan_config; 
    rmt_encoder_handle_t led_encoder;
    rmt_transmit_config_t tx_config;
    uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

public:
    Ledstrip();

    esp_err_t init();
    void colorful1(uint32_t delay_ms);
    void monocolor(uint8_t red, uint8_t green, uint8_t blue);
    void rainbow(uint32_t startled, uint32_t bright);
};
