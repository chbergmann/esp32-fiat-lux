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
#define EXAMPLE_LED_NUMBERS         CONFIG_LED_NUMBERS
#define MAX_LEDS 10000
#define SPEED_MAX_VAL   100
#define PERIOD_SECOND   1000
#define PERIOD_MIN      10

static const char *TAG = "leds";

Ledstrip::Ledstrip(const char *spiffs_path)
{
    snprintf(cfgfile_path, sizeof(cfgfile_path), "%s/config.bin", spiffs_path);
    memset(&cfg, 0, sizeof(led_config_t));
    led_strip_pixels = NULL;
    rmt_pixels = NULL;
    mainTask = 0;
    lastSec = -1;
    startled = 0;

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
    new_led_strip_pixels(0);
}

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
    if(fwrite(led_strip_pixels, 1, led_strip_size(), f) != led_strip_size())
    {
        ESP_LOGE(TAG, "Failed to write to %s: %s", cfgfile_path, strerror(errno));
    }
    fclose(f);
}

void Ledstrip::restoreConfig()
{
    memset(&cfg, 0, sizeof(led_config_t));
    cfg.num_leds = EXAMPLE_LED_NUMBERS;
    cfg.bright = 100;
    cfg.color1.red = 255;
    cfg.color2.blue = 255;
    cfg.gradients = 1;
    cfg.power = true;

    FILE* f = fopen(cfgfile_path, "r");
    if (f == NULL) 
    {
        ESP_LOGW(TAG, "Failed to open %s. Using default config", cfgfile_path);
        new_led_strip_pixels(cfg.num_leds);
    }
    else {
        if(fread(&cfg, 1, sizeof(cfg), f) != sizeof(cfg))
        {
            ESP_LOGE(TAG, "Failed to read %s. Using default config", cfgfile_path);
        }
        if(cfg.num_leds < MAX_LEDS)
        {
            new_led_strip_pixels(cfg.num_leds);
            if(fread(led_strip_pixels, 1, led_strip_size(), f) != led_strip_size())
            {
                ESP_LOGE(TAG, "Failed to read %s. Using default config", cfgfile_path);
            }
        }
        fclose(f);
    }
    startled = cfg.led1;
}

void Ledstrip::dark()
{
    memset(led_strip_pixels, 0, led_strip_size());
}

void Ledstrip::walk()
{
    if(cfg.speed == 0)
        return;
        
    int i = cfg.num_leds - 1;
    color_t buf = led_strip_pixels[i];

    for(i=cfg.num_leds-2; i>=0; i--)
    {
        led_strip_pixels[(i+1)] = led_strip_pixels[i];
    }
    led_strip_pixels[0] = buf;
}

void Ledstrip::firstled(color_t color)
{
    led_strip_pixels[cfg.led1 % cfg.num_leds] = color;
    cfg.gradients = 1;
}

string Ledstrip::to_json(led_config_t& cfg)
{
    return "{" + 
        to_json("red", cfg.color1.red) + "," + 
        to_json("green", cfg.color1.green) + "," + 
        to_json("blue", cfg.color1.blue) + "," + 
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

void Ledstrip::new_led_strip_pixels(uint32_t nr_leds)
{
    if(led_strip_pixels)
        delete led_strip_pixels;

    if(rmt_pixels)
        delete rmt_pixels;

    if(nr_leds > 0)
    {
        led_strip_pixels = new color_t[nr_leds];
        rmt_pixels = new uint8_t[nr_leds*3];
    }
    cfg.num_leds = nr_leds;
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
    for (int j = 0; j < cfg.num_leds; j ++) 
    {
        led_strip_pixels[j] = cfg.color1;
    }
}


void Ledstrip::add_gradient(color_t color)
{
    for(int g=1; g<cfg.gradients-1; g++)
    {
        int a = cfg.num_leds * g / cfg.gradients;
        int b = (cfg.num_leds * (g + 1) / cfg.gradients);
        if(a == b)
        {
            ESP_LOGW(TAG, "not enough LEDs");
            return;
        }
        led_strip_pixels[b] = led_strip_pixels[a];
    }
    cfg.gradients++;
    led_strip_pixels[cfg.num_leds * (cfg.gradients - 1) / cfg.gradients] = color;
}

uint8_t Ledstrip::get_gradient(uint8_t color1, uint8_t color2, int a, int b, int i)
{
    uint8_t ret;
    if(color2 >= color1)
        ret = color1 + (color2 - color1) * (b - i) / (b - a);
    else
        ret = color1 - (color1 - color2) * (b - i) / (b - a);

    return ret;
}

void Ledstrip::gradient()
{
    int gradients = cfg.gradients;
    if(gradients < 2)
        gradients = 2;

    for(int g=0; g<gradients; g++)
    {     
        int a = cfg.num_leds * g / gradients;
        int b = (cfg.num_leds * (g + 1) / gradients);
        for (int i = a + 1; i < b; i++) 
        {
            led_strip_pixels[i].green = get_gradient(led_strip_pixels[b % cfg.num_leds].green, led_strip_pixels[a].green, a, b, i);
            led_strip_pixels[i].red = get_gradient(led_strip_pixels[b % cfg.num_leds].red, led_strip_pixels[a].red, a, b, i);
            led_strip_pixels[i].blue = get_gradient(led_strip_pixels[b % cfg.num_leds].blue, led_strip_pixels[a].blue, a, b, i);
        }
    }

    if(cfg.speed > 0)
        startled = (startled + 1) % cfg.num_leds;   
}

void Ledstrip::rainbow()
{
    uint16_t hue = 0;
    uint32_t red, green, blue;
    
    for (int j = 0; j < cfg.num_leds; j ++) 
    {
        hue = (cfg.num_leds + j) % cfg.num_leds * 360 / cfg.num_leds;
        led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
        led_strip_pixels[j].green = green * cfg.bright / 100;
        led_strip_pixels[j].red = red * cfg.bright / 100;
        led_strip_pixels[j].blue = blue * cfg.bright / 100;
    }
    
    if(cfg.speed > 0)
        startled = (startled + 1) % cfg.num_leds;    
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
    ESP_LOGI(TAG, "%s hue %d", strftime_buf, hue);

    hue = 359 - (now * 360 / (24*60*60) % 360);
    led_strip_hsv2rgb(hue, 100, cfg.bright, &red, &green, &blue);
    
    for (int j = 0; j < cfg.num_leds; j ++) 
    {
        led_strip_pixels[j].green = green;
        led_strip_pixels[j].red = red;
        led_strip_pixels[j].blue = blue;
    }
}

void Ledstrip::clock2()
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    uint16_t hue = 0;
    uint32_t red, green, blue;

    time(&now);
    localtime_r(&now, &timeinfo);
    if(lastSec != timeinfo.tm_sec)
    {
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "%s", strftime_buf);
        lastSec = timeinfo.tm_sec;
    }
    
    dark();
    
    hue = 359 - (now * 360 / (24*60*60) % 360);
    led_strip_hsv2rgb(hue, 100, 5, &red, &green, &blue);
    
    uint32_t minuteleds = (cfg.num_leds * timeinfo.tm_min / 60 + cfg.led1) % cfg.num_leds;
    for (int j = 0; j < minuteleds; j ++)
    {
        led_strip_pixels[j].green = green;
        led_strip_pixels[j].red = red;
        led_strip_pixels[j].blue = blue;
    }

    if(cfg.num_leds >= 60)
    {
        for (int i = 0; i < cfg.num_leds; i += cfg.num_leds/12)
        {
            led_strip_pixels[i] = cfg.color2;
        }
    }

    uint32_t hourleds = (cfg.num_leds * timeinfo.tm_hour / 24 + cfg.led1) % cfg.num_leds;
    int nr_leds = cfg.num_leds / 60;
    nr_leds = nr_leds * 2 + 1;
    hourleds -= nr_leds / 2;
    led_strip_hsv2rgb(hue, 100, 100, &red, &green, &blue);
    for (int i = 0; i < nr_leds; i ++)
    {
        led_strip_pixels[hourleds + i].green = green;
        led_strip_pixels[hourleds + i].red = red;
        led_strip_pixels[hourleds + i].blue = blue;
    }

    uint32_t secleds = (cfg.num_leds * timeinfo.tm_sec / 60 + cfg.led1) % cfg.num_leds;
    led_strip_pixels[secleds] = cfg.color1;
}

void Ledstrip::switchLeds()
{
    switch(cfg.algorithm)
    {
        case ALGO_MONO: monocolor(); break;
        case ALGO_RAINBOW: rainbow(); break;
        case ALGO_RAINBOWCLK: rainbow_clock(); break;
        case ALGO_WALK: walk(); break;
        case ALGO_CLOCK2: clock2(); break;
        case ALGO_GRADIENT: gradient(); break;
    }

    if(!cfg.power)
    {
        memset(rmt_pixels, 0, led_strip_size());
    }
    else 
    {
        for(int i=0; i<cfg.num_leds; i++)
        {
            int j = cfg.counterclock ? cfg.num_leds - 1 - i : i;
            int n = (i + startled) * 3 % led_strip_size();
            rmt_pixels[n + 0] = led_strip_pixels[j].green;
            rmt_pixels[n + 1] = led_strip_pixels[j].red;
            rmt_pixels[n + 2] = led_strip_pixels[j].blue;
        }
    }

    ESP_ERROR_CHECK(rmt_transmit(led_chan, led_encoder, rmt_pixels, led_strip_size(), &tx_config));
}

void Ledstrip::loop()
{
    mainTask = xTaskGetCurrentTaskHandle();
    for (;;)
    {
        TickType_t period;
        TickType_t lastWakeTime = xTaskGetTickCount();
        switchLeds();  
        rmt_tx_wait_all_done(led_chan, PERIOD_SECOND);
        
        switch(cfg.algorithm)
        {
            case ALGO_RAINBOW:
            case ALGO_WALK:
            case ALGO_GRADIENT:
                period = 10 * PERIOD_SECOND * (SPEED_MAX_VAL + 1 - cfg.speed) / SPEED_MAX_VAL / cfg.num_leds;
                break;

            case ALGO_CLOCK2:
                period = PERIOD_SECOND / cfg.num_leds;
                break;

            default: 
                period = PERIOD_SECOND; 
                break;
        }

        TickType_t diff = xTaskGetTickCount() - lastWakeTime;
        if(pdMS_TO_TICKS(period) > diff)
            vTaskDelay(pdMS_TO_TICKS(period) - diff);
    }
}

void Ledstrip::switchNow()
{
    cfg.power = true;
    xTaskAbortDelay( mainTask );
}

void Ledstrip::onoff()
{
    cfg.power = !cfg.power;
    xTaskAbortDelay( mainTask );
}
