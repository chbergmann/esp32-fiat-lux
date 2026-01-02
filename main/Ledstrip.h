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
} ledstrip_algo_t;

typedef struct 
{
    uint32_t num_leds;
    uint32_t led1;
    bool counterclock;
    ledstrip_algo_t algorithm;
    uint32_t red;
    uint32_t green;
    uint32_t blue;
    uint32_t bright;
    uint32_t speed;
} led_config_t;

class Ledstrip {
    rmt_channel_handle_t led_chan;
    rmt_tx_channel_config_t tx_chan_config; 
    rmt_encoder_handle_t led_encoder;
    rmt_transmit_config_t tx_config;
    uint8_t* led_strip_pixels;
    uint8_t* rev_pixels;
    uint32_t loopcnt;
    char cfgfile_path[32];
    uint32_t startled;

    string to_json(const string& tag, uint32_t nr);

public:
    led_config_t cfg;

    Ledstrip(const char* spiffs_path);
    ~Ledstrip();
    void loop();

    esp_err_t init();
    void switchLeds();
    void saveConfig();
    void restoreConfig();

    // LED algorithms
    void monocolor();
    void rainbow();
    void rainbow_clock();
    void dark();
    void walk();
    void firstled(uint32_t red, uint32_t green, uint32_t blue);

    string to_json(led_config_t& cfg);
};
