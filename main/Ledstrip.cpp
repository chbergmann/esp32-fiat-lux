/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "esp_log.h"
#include "led_strip_encoder.h"
#include "Ledstrip.h"
#include <cmath>

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define TASK_PERIOD_MS  10
static const char *TAG = "leds";


/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

Ledstrip::Ledstrip()
{
    red = 0;
    green = 0;
    blue = 0;
    bright = 100;
    startled = 0;
    loopcnt = 0;
    algorithm = ALGO_MONO;

    led_chan = NULL;
    led_encoder = NULL;
    memset(&tx_chan_config, 0, sizeof(tx_chan_config));
    memset(&tx_config, 0, sizeof(tx_config));
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT; // select source clock
    tx_chan_config.gpio_num = (gpio_num_t)RMT_LED_STRIP_GPIO_NUM;
    tx_chan_config.mem_block_symbols = 64; // increase the block size can make the LED less flickering
    tx_chan_config.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ;
    tx_chan_config.trans_queue_depth = 4; // set the number of transactions that can be pending in the background
    tx_config.loop_count = 0; // no transfer loop
}

esp_err_t Ledstrip::init()
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(led_chan));
    switchLeds();
    return ESP_OK;
}

void Ledstrip::monocolor()
{
    ESP_LOGI(TAG, "R=%d G=%d B=%d", red, green, blue);
    for (int j = 0; j < EXAMPLE_LED_NUMBERS; j ++) {
        // Build RGB pixels
        led_strip_pixels[j * 3 + 0] = green;
        led_strip_pixels[j * 3 + 1] = red;
        led_strip_pixels[j * 3 + 2] = blue;
    }
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    //ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
    ESP_LOGI(TAG, "RGB done");
}

void Ledstrip::rainbow()
{
    uint16_t hue = 0;
    
    for (int j = 0; j < EXAMPLE_LED_NUMBERS; j ++) {
        // Build RGB pixels
        hue = (EXAMPLE_LED_NUMBERS + j - startled) % EXAMPLE_LED_NUMBERS * 360 / EXAMPLE_LED_NUMBERS;
        led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
        led_strip_pixels[j * 3 + 0] = green * bright / 100;
        led_strip_pixels[j * 3 + 1] = red * bright / 100;
        led_strip_pixels[j * 3 + 2] = blue * bright / 100;
    }
    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &tx_config));
    //ESP_ERROR_CHECK(rmt_tx_wait_all_done(led_chan, portMAX_DELAY));
}

void Ledstrip::switchLeds()
{
    switch(algorithm)
    {
        case ALGO_MONO: monocolor(); break;
        case ALGO_RAINBOW: rainbow(); break;
    }
}

void Ledstrip::loop()
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    // Convert 10 ms to ticks
    const TickType_t period = pdMS_TO_TICKS(TASK_PERIOD_MS);

    for (;;)
    {
        if(speed != 0)
        {
            loopcnt++;
            int32_t s = pow(2, (10 - speed));
            if(loopcnt >= s) {
                startled = (startled + 1) % EXAMPLE_LED_NUMBERS;
                switchLeds();       
                loopcnt = 0;
            }
        }

        // Wait until the next cycle
        vTaskDelayUntil(&lastWakeTime, period);
    }
}