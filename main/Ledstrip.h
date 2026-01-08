#pragma once

#include <string>
#include "RmtTxDriver.h"

using namespace std;

typedef enum {
    ALGO_MONO,
    ALGO_GRADIENT,
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
    color_t  color2;
    uint32_t bright;
    uint32_t speed;
    uint32_t gradients;
    bool power;
    char name[16];
} led_config_t;

class Ledstrip {
    color_t* led_strip_pixels;
    uint8_t* rmt_pixels;
    char cfgfile_path[32];
    uint32_t startled;
    TaskHandle_t mainTask;
    int lastSec;
    RmtTxDriver* rmt;
    gpio_num_t gpio_nr;

    void new_led_strip_pixels(uint32_t nr_leds);
    size_t led_strip_size() { return cfg.num_leds * 3; }
    void switchLeds();
    static uint8_t get_gradient(uint8_t color1, uint8_t color2, int a, int b, int i);
    int in_range(int lednr);

public:
    led_config_t cfg;

    Ledstrip();
    ~Ledstrip();
    void loop();

    esp_err_t init(const char* spiffs_path, RmtTxDriver* rmt_inst, gpio_num_t gpionr);
    void saveConfig();
    void restoreConfig();
    void switchNow();
    void onoff();

    // LED algorithms
    void monocolor();
    void rainbow();
    void rainbow_clock();
    void dark();
    void walk();
    void firstled(color_t color);
    void clock2();
    void gradient();
    void add_gradient(color_t color);

    string to_json(led_config_t& cfg);
    static string to_json(const string& tag, uint32_t nr);
};
