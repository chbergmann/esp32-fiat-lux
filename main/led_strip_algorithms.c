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
#include "led_strip_algorithms.h"

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      CONFIG_LED_STRIP_GPIO_NUM
#define EXAMPLE_LED_NUMBERS         CONFIG_LED_NUMBERS

static const char *TAG = "leds";

static uint8_t led_strip_pixels[EXAMPLE_LED_NUMBERS * 3];

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

void init_ledstrip(led_strip_t* ledstrip)
{
    ESP_LOGI(TAG, "Create RMT TX channel");
    ledstrip->led_chan = NULL;
    ledstrip->led_encoder = NULL;
    rmt_tx_channel_config_t tx_chan_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = RMT_LED_STRIP_GPIO_NUM,
        .mem_block_symbols = 64, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
    };
    ledstrip->tx_chan_config = tx_chan_config;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&ledstrip->tx_chan_config, &ledstrip->led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &ledstrip->led_encoder));

    ESP_LOGI(TAG, "Enable RMT TX channel");
    ESP_ERROR_CHECK(rmt_enable(ledstrip->led_chan));

    rmt_transmit_config_t tx_config = {
        .loop_count = 0, // no transfer loop
    };
    ledstrip->tx_config = tx_config;
}

void led_strip_colorful1(led_strip_t* ledstrip, uint32_t delay_ms)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;

    ESP_LOGI(TAG, "Start LED rainbow chase");
    while (1) {
        for (int i = 0; i < 3; i++) {
            for (int j = i; j < EXAMPLE_LED_NUMBERS; j += 3) {
                // Build RGB pixels
                hue = j * 360 / EXAMPLE_LED_NUMBERS + start_rgb;
                led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
                led_strip_pixels[j * 3 + 0] = green;
                led_strip_pixels[j * 3 + 1] = blue;
                led_strip_pixels[j * 3 + 2] = red;
            }
            // Flush RGB values to LEDs
            ESP_ERROR_CHECK(rmt_transmit(ledstrip->led_chan, ledstrip->led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &ledstrip->tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(ledstrip->led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            memset(led_strip_pixels, 0, sizeof(led_strip_pixels));
            ESP_ERROR_CHECK(rmt_transmit(ledstrip->led_chan, ledstrip->led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &ledstrip->tx_config));
            ESP_ERROR_CHECK(rmt_tx_wait_all_done(ledstrip->led_chan, portMAX_DELAY));
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
        start_rgb += 60;
    }
}

void led_monocolor(led_strip_t* ledstrip, uint8_t red, uint8_t green, uint8_t blue)
{
    for (int j = 0; j < EXAMPLE_LED_NUMBERS; j ++) {
        // Build RGB pixels
        led_strip_pixels[j * 3 + 0] = green;
        led_strip_pixels[j * 3 + 1] = red;
        led_strip_pixels[j * 3 + 2] = blue;
    }
    ESP_ERROR_CHECK(rmt_transmit(ledstrip->led_chan, ledstrip->led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &ledstrip->tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(ledstrip->led_chan, portMAX_DELAY));
}

void led_rainbow(led_strip_t* ledstrip, uint32_t startled, uint32_t bright)
{
    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;
    
    for (int j = 0; j < EXAMPLE_LED_NUMBERS; j ++) {
        // Build RGB pixels
        hue = (j - startled) * 360 / EXAMPLE_LED_NUMBERS + start_rgb;
        led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
        led_strip_pixels[j * 3 + 0] = green * bright / 100;
        led_strip_pixels[j * 3 + 1] = red * bright / 100;
        led_strip_pixels[j * 3 + 2] = blue * bright / 100;
    }
    ESP_ERROR_CHECK(rmt_transmit(ledstrip->led_chan, ledstrip->led_encoder, led_strip_pixels, sizeof(led_strip_pixels), &ledstrip->tx_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(ledstrip->led_chan, portMAX_DELAY));
}