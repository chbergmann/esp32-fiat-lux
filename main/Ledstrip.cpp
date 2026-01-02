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
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define TASK_PERIOD_MS  10
#define EXAMPLE_LED_NUMBERS         CONFIG_LED_NUMBERS

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

void Ledstrip::saveConfig()
{
    FILE* f = fopen(cfgfile_path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s for writing", cfgfile_path);
        return;
    }
    if(fwrite(&cfg, 1, sizeof(cfg), f) != sizeof(cfg))
    {
        ESP_LOGE(TAG, "Failed to write to %s: %s", cfgfile_path, strerror(errno));
    }
    fclose(f);
}

void Ledstrip::restoreConfig()
{
    if(led_strip_pixels) 
    {
        delete led_strip_pixels;
        led_strip_pixels = NULL;
        delete rev_pixels;
        rev_pixels = NULL;
    }
    memset(&cfg, 0, sizeof(led_config_t));
    cfg.num_leds = EXAMPLE_LED_NUMBERS;
    cfg.bright = 100;
    cfg.red = 255;

    FILE* f = fopen(cfgfile_path, "r");
    if (f == NULL) 
    {
        ESP_LOGW(TAG, "Failed to open %s. Using default config", cfgfile_path);
    }
    else {
        if(fread(&cfg, 1, sizeof(cfg), f) != sizeof(cfg))
        {
            ESP_LOGE(TAG, "Failed to read %s. Using default config", cfgfile_path);
        }
        fclose(f);
    }
        
    led_strip_pixels = new uint8_t[cfg.num_leds * 3];
    rev_pixels = new uint8_t[cfg.num_leds * 3];
}

void Ledstrip::dark()
{
    memset(led_strip_pixels, 0, cfg.num_leds * 3);
}

void Ledstrip::walk()
{
    if(loopcnt != 1)
        return;

    int i = cfg.num_leds - 1;
    uint8_t buf[3];
    for(int j=0; j<3; j++)
        buf[j] = led_strip_pixels[i * 3 + j];

    for(i=cfg.num_leds-2; i>=0; i--)
    {
        for(int j=0; j<3; j++)
        {
            led_strip_pixels[(i+1) * 3 + j] = led_strip_pixels[i * 3 + j];
        }
    }
    for(int j=0; j<3; j++)
        led_strip_pixels[j] = buf[j];
}

void Ledstrip::firstled(uint32_t red, uint32_t green, uint32_t blue)
{
    int n = (cfg.led1 % cfg.num_leds) * 3;
    led_strip_pixels[n + 0] = green;
    led_strip_pixels[n + 1] = red;
    led_strip_pixels[n + 2] = blue;
}

string Ledstrip::to_json(led_config_t& cfg)
{
    return "{" + 
        to_json("red", cfg.red) + "," + 
        to_json("green", cfg.green) + "," + 
        to_json("blue", cfg.blue) + "," + 
        to_json("speed", cfg.speed) + "," + 
        to_json("bright", cfg.bright) + "," +
        to_json("nr_leds", cfg.num_leds) + "," +
        to_json("led1", cfg.led1) + "," +
        "\"rotate\":" + (cfg.counterclock ? "\"left\"" : "\"right\"") +
        "}";
}

string Ledstrip::to_json(const string &tag, uint32_t nr)
{
    return "\"" + tag + "\":" + to_string(nr);
}

Ledstrip::Ledstrip(const char *spiffs_path)
{
    snprintf(cfgfile_path, sizeof(cfgfile_path), "%s/config.bin", spiffs_path);
    memset(&cfg, 0, sizeof(led_config_t));
    led_strip_pixels = NULL;
    rev_pixels = NULL;

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

Ledstrip::~Ledstrip()
{
    if(led_strip_pixels)
        delete led_strip_pixels;

    if(rev_pixels)
        delete rev_pixels;
}

esp_err_t Ledstrip::init()
{
    restoreConfig();
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
    //ESP_LOGI(TAG, "R=%d G=%d B=%d", cfg.red, cfg.green, cfg.blue);
    for (int j = 0; j < cfg.num_leds; j ++) {
        // Build RGB pixels
        led_strip_pixels[j * 3 + 0] = cfg.green;
        led_strip_pixels[j * 3 + 1] = cfg.red;
        led_strip_pixels[j * 3 + 2] = cfg.blue;
    }
}

void Ledstrip::rainbow()
{
    uint16_t hue = 0;
    uint32_t red, green, blue;
    
    if(loopcnt == 0)
    {
        for (int j = 0; j < cfg.num_leds; j ++) {
            // Build RGB pixels
            hue = (cfg.num_leds + j - startled) % cfg.num_leds * 360 / cfg.num_leds;
            led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
            led_strip_pixels[j * 3 + 0] = green * cfg.bright / 100;
            led_strip_pixels[j * 3 + 1] = red * cfg.bright / 100;
            led_strip_pixels[j * 3 + 2] = blue * cfg.bright / 100;
        }
    }
    else if(loopcnt == 1) 
    {
        startled = (startled + 1) % cfg.num_leds;    
    }
}

void Ledstrip::rainbow_clock()
{
    uint16_t hue = 0;
    uint32_t red, green, blue;

    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    hue = now * 360 / (24*60*60) % 360;
    ESP_LOGI(TAG, "%s hue %d", strftime_buf, hue);
    led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
    
    for (int j = 0; j < cfg.num_leds; j ++) {
        // Build RGB pixels
        led_strip_pixels[j * 3 + 0] = green * cfg.bright / 100;
        led_strip_pixels[j * 3 + 1] = red * cfg.bright / 100;
        led_strip_pixels[j * 3 + 2] = blue * cfg.bright / 100;
    }
}

void Ledstrip::switchLeds()
{
    switch(cfg.algorithm)
    {
        case ALGO_MONO: monocolor(); break;
        case ALGO_RAINBOW: rainbow(); break;
        case ALGO_RAINBOWCLK: rainbow_clock(); break;
        case ALGO_WALK: walk(); break;
    }

    if(!cfg.counterclock)
    {
        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, led_strip_pixels, cfg.num_leds*3, &tx_config));
    }
    else
    {
        for(int i=0; i<cfg.num_leds; i++)
        {
            rev_pixels[i * 3 + 0] = led_strip_pixels[cfg.num_leds*3 - (i * 3) - 3];
            rev_pixels[i * 3 + 1] = led_strip_pixels[cfg.num_leds*3 - (i * 3) - 2];
            rev_pixels[i * 3 + 2] = led_strip_pixels[cfg.num_leds*3 - (i * 3) - 1];
        }

        ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, rev_pixels, cfg.num_leds*3, &tx_config));
    }
}

void Ledstrip::loop()
{
    TickType_t lastWakeTime = xTaskGetTickCount();

    // Convert 10 ms to ticks
    TickType_t period;

    for (;;)
    {
        switchLeds();  
        period = 1000;     

        if(cfg.speed > 0)
        {
            loopcnt++;
            int32_t s = pow(2, (10 - cfg.speed));
            if(loopcnt >= s)
                loopcnt = 0;
                
            // Wait until the next cycle
            if(cfg.algorithm == ALGO_RAINBOW || cfg.algorithm == ALGO_WALK)
                period = TASK_PERIOD_MS;
        }
        else {
            loopcnt = 0;
        }
            
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(period));
    }
}
