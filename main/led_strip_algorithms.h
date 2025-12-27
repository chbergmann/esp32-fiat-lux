#pragma once

#include "driver/rmt_tx.h"

typedef struct {
    rmt_channel_handle_t led_chan;
    rmt_tx_channel_config_t tx_chan_config; 
    rmt_encoder_handle_t led_encoder;
    rmt_transmit_config_t tx_config;
} led_strip_t;

void init_ledstrip(led_strip_t* ledstrip);
void led_strip_colorful1(led_strip_t* ledstrip, uint32_t delay_ms);
void led_monocolor(led_strip_t* ledstrip, uint8_t red, uint8_t green, uint8_t blue);
void led_rainbow(led_strip_t* ledstrip, uint32_t startled);