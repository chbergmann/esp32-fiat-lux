#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/rmt_tx.h"

class RmtTxDriver {
    rmt_channel_handle_t led_chan;
    rmt_tx_channel_config_t tx_chan_config; 
    rmt_encoder_handle_t led_encoder;
    rmt_transmit_config_t tx_config;
    SemaphoreHandle_t mutex;

public:
    RmtTxDriver();
    ~RmtTxDriver();

    esp_err_t init(gpio_num_t gpionr);
    esp_err_t transmit(gpio_num_t gpionr, uint8_t* pixels, size_t nr_pixels, int timeout_ms);
    esp_err_t transmit(uint8_t* pixels, size_t nr_pixels, int timeout_ms);
};