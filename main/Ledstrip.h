#pragma once

#include "driver/rmt_tx.h"
#include <string>

using namespace std;

#define RMT_LED_STRIP_GPIO_NUM      CONFIG_LED_STRIP_GPIO_NUM

typedef enum {
    ALGO_MONO,
    ALGO_RAINBOW,
    ALGO_RAINBOWCLK,
    ALGO_WALK,
    ALGO_CLOCK2,
} ledstrip_algo_t;

typedef struct {
    uint8_t green;
    uint8_t red;
    uint8_t blue;
} color_t;


typedef struct 
{
    uint32_t num_leds;
    uint32_t led1;
    bool counterclock;
    ledstrip_algo_t algorithm;
    color_t  color1;
    uint32_t bright;
    uint32_t speed;
} led_config_t;

class Ledstrip {
    rmt_channel_handle_t led_chan;
    rmt_tx_channel_config_t tx_chan_config; 
    rmt_encoder_handle_t led_encoder;
    rmt_transmit_config_t tx_config;
    color_t* led_strip_pixels;
    uint8_t* rmt_pixels;
    char cfgfile_path[32];
    uint32_t startled;
    TaskHandle_t mainTask;

    string to_json(const string& tag, uint32_t nr);
    void new_led_strip_pixels(uint32_t nr_leds);
    size_t led_strip_size() { return cfg.num_leds * 3; }
    void switchLeds();

public:
    led_config_t cfg;

    Ledstrip(const char* spiffs_path);
    ~Ledstrip();
    void loop();

    esp_err_t init();
    void saveConfig();
    void restoreConfig();
    void switchNow();

    // LED algorithms
    void monocolor();
    void rainbow();
    void rainbow_clock(uint32_t brightness);
    void dark();
    void walk();
    void firstled(color_t color);
    void clock2();

    string to_json(led_config_t& cfg);
};
