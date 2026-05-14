/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "esp_log.h"
#include "Ledstrip.h"
#include <cmath>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <cstdlib>

#define EXAMPLE_LED_NUMBERS         CONFIG_LED_NUMBERS
#define MAX_LEDS 10000
#define SPEED_MAX_VAL   100
#define PERIOD_SECOND   1000
#define PERIOD_MIN      10
#define STACK_SIZE      CONFIG_ESP_MAIN_TASK_STACK_SIZE

static const char *TAG = "leds";

void c_monocolor(Ledstrip* pL)      { pL->monocolor(); }
void c_rainbow(Ledstrip* pL)        { pL->rainbow(); }
void c_rainbow_clock(Ledstrip* pL)  { pL->rainbow_clock(); }
void c_walk(Ledstrip* pL)           { pL->walk(); }
void c_clock2(Ledstrip* pL)         { pL->clock2(); }
void c_gradient(Ledstrip* pL)       { pL->gradient(); }
void c_belt(Ledstrip* pL)           { pL->belt(); }
void c_fire(Ledstrip* pL)           { pL->fire(); }

const ledfunc_table_t Ledstrip::ledfunc_table[] = {
        { ALGO_MONO,        "/mono",        c_monocolor },
        { ALGO_RAINBOW,     "/rainbow",     c_rainbow },
        { ALGO_RAINBOWCLK,  "/rainbowclk",  c_rainbow_clock },
        { ALGO_WALK,        "/walk",        c_walk },
        { ALGO_CLOCK2,      "/clock2",      c_clock2 },
        { ALGO_GRADIENT,    "/gradient",    c_gradient },
        { ALGO_BELT,        "/belt",        c_belt },
        { ALGO_FIRE,        "/fire",        c_fire },
        { ALGO_END,              "",             nullptr },
};

Ledstrip::Ledstrip()
{
    memset(&cfg, 0, sizeof(led_config_t));
    led_strip_pixels = NULL;
    rmt_pixels = NULL;
    mainTask = 0;
    lastSec = -1;
    startled = 0;
    cfgfile_path[0] = 0;
    rmt = NULL;
    fade_in = 0;
    startTime = 0;
}

Ledstrip::~Ledstrip()
{
    mainTask = 0;
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
    sprintf(cfg.name, "Strip GPIO%d", gpio_nr);

    FILE* f = fopen(cfgfile_path, "r");
    if (f == NULL) 
    {
        ESP_LOGW(TAG, "Failed to open %s. Using default config", cfgfile_path);
        new_led_strip_pixels(cfg.num_leds);
    }
    else 
    {
        led_config_t fcfg;
        if(fread(&fcfg, 1, sizeof(led_config_t), f) != sizeof(led_config_t))
        {
            ESP_LOGE(TAG, "Failed to read %s. Using default config", cfgfile_path);
        }
        if(fcfg.num_leds < MAX_LEDS)
        {
            memcpy(&cfg, &fcfg, sizeof(led_config_t));
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
    led_strip_pixels[0] = color;
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
        to_json("rotate", (cfg.counterclock ? "left" : "right")) + "," +
        to_json("name", cfg.name) + "," +
        to_json("fadein", cfg.fadein_ms) +
        "}";
}

string Ledstrip::to_json(const string &tag, uint32_t nr)
{
    return "\"" + tag + "\":" + to_string(nr);
}

string Ledstrip::to_json(const string &tag, const string& str)
{
    return "\"" + tag + "\":" + "\"" + str + "\"";
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
    ESP_LOGI(TAG, "Nr. LEDs: %d", cfg.num_leds);
}

void vLedstripTask( void * pvParameters )
{
    Ledstrip* ls = (Ledstrip*)pvParameters;
    ls->loop();
}

esp_err_t Ledstrip::init(const char *spiffs_path, RmtTxDriver* rmt_inst, gpio_num_t gpionr)
{
    rmt = rmt_inst;
    gpio_nr = gpionr;
    snprintf(cfgfile_path, sizeof(cfgfile_path), "%s/config%d.bin", spiffs_path, gpionr);
    restoreConfig();

    BaseType_t xReturned;
    TaskHandle_t xHandle = NULL;

    /* Create the task, storing the handle. */
    xReturned = xTaskCreate(
                    vLedstripTask    ,       /* Function that implements the task. */
                    "LedstripTask",          /* Text name for the task. */
                    STACK_SIZE,      /* Stack size in words, not bytes. */
                    this,    /* Parameter passed into the task. */
                    1,      /* Priority at which the task is created. */
                    &xHandle );      /* Used to pass out the created task's handle. */

    if( xReturned != pdPASS )
    {
        ESP_LOGE(TAG, "could not create a task for ledstrip at GPIO %d", gpionr);
        return ESP_FAIL;
    }
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

uint8_t Ledstrip::colorchange1(uint8_t ledcol)
{
    return ledcol * (rand() % 256) / 256;
}

void Ledstrip::belt()
{
    for(int i=0; i<(cfg.num_leds+1)/2 + 1; i++)
    {
        led_strip_pixels[i] = led_strip_pixels[i+1];
        led_strip_pixels[cfg.num_leds - i] = led_strip_pixels[i];
    }
    int m = cfg.num_leds/2;
    led_strip_pixels[m].red = colorchange1(cfg.color1.red);
    led_strip_pixels[m].green = colorchange1(cfg.color1.green);
    led_strip_pixels[m].blue = colorchange1(cfg.color1.blue);
    startled = cfg.led1;
}

void Ledstrip::fire()
{
    for(int i=0; i<cfg.num_leds; i++)
    {
        led_strip_pixels[i].red = 127 + rand() % 128;
        led_strip_pixels[i].green = 33 + rand() % 66;
        led_strip_pixels[i].blue = 0;
    }
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

int Ledstrip::in_range(int lednr)
{
    if(lednr < 0)
        lednr += cfg.num_leds;

    return lednr % cfg.num_leds;
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
    struct timeval tv_now;

    time(&now);
    gettimeofday(&tv_now, NULL);
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
    
    uint32_t minuteleds = cfg.num_leds * timeinfo.tm_min / 60;
    for (int j = 0; j < minuteleds; j ++)
    {
        led_strip_pixels[j].green = green;
        led_strip_pixels[j].red = red;
        led_strip_pixels[j].blue = blue;
    }

    if(cfg.num_leds >= 60)
    {
        for (int i = 0; i < 12; i++)
        {
            int w;
            if(i == 0)
                w = 2;
            else if(i % 3 == 0)
                w = 1;
            else
                w = 0;

            for (int n = -w; n <= w; n ++)
            {
                int l = cfg.num_leds * i / 12 + n;
                led_strip_pixels[in_range(l)] = cfg.color2;
            }
        }
    }

    uint32_t hourleds = cfg.num_leds * ((timeinfo.tm_hour%12) * 60 + timeinfo.tm_min) / (12*60);
    led_strip_hsv2rgb(hue + 180, 100, 100, &red, &green, &blue);
    for (int i = -1; i <= 1; i ++)
    {
        int l = in_range(hourleds + i);
        led_strip_pixels[l].green = green;
        led_strip_pixels[l].red = red;
        led_strip_pixels[l].blue = blue;
    }

    uint32_t secleds = cfg.num_leds * ((tv_now.tv_sec % 60) * 1000 + tv_now.tv_usec / 1000) / 60000;
    led_strip_pixels[secleds] = cfg.color1;
    startled = cfg.led1;
}

void Ledstrip::switchLeds()
{
    if(cfg.num_leds == 0)
        return;

    for(int i=0; ledfunc_table[i].algo != ALGO_END; i++)
    {
        if(cfg.algorithm == ledfunc_table[i].algo)
        {
            ledfunc_table[i].func(this);
            break;
        }
    }
    transmit();
}

void Ledstrip::transmit()
{
    if(cfg.num_leds == 0)
        return;

    if(!cfg.power)
    {
        memset(rmt_pixels, 0, led_strip_size());
    }
    else 
    {
        uint32_t frac;
        if(cfg.fadein_ms == 0 || fade_in == 0)
        {
            frac = 1;
            fade_in = 1;
        }
        else if(fade_in < cfg.fadein_ms) 
            frac = cfg.fadein_ms;
        else
            frac = fade_in;

        for(int i=0; i<cfg.num_leds; i++)
        {
            int j = cfg.counterclock ? cfg.num_leds - 1 - i : i;
            int n = in_range(i + startled) * 3;
            rmt_pixels[n + 0] = led_strip_pixels[j].green * fade_in / frac;
            rmt_pixels[n + 1] = led_strip_pixels[j].red * fade_in / frac;
            rmt_pixels[n + 2] = led_strip_pixels[j].blue * fade_in / frac;
        }
    }

    if(rmt)
    {
        rmt->transmit(gpio_nr, rmt_pixels, led_strip_size(), PERIOD_SECOND);
    }
}

void Ledstrip::loop()
{
    startTime = xTaskGetTickCount();
    mainTask = xTaskGetCurrentTaskHandle();
    ESP_LOGI(TAG, "started LED strip task at GPIO %d", gpio_nr);
    while(mainTask)
    {
        TickType_t period;
        TickType_t lastWakeTime = xTaskGetTickCount();
        fade_in = pdTICKS_TO_MS(lastWakeTime - startTime);
        switchLeds();  
        
        switch(cfg.algorithm)
        {
            case ALGO_RAINBOW:
            case ALGO_WALK:
            case ALGO_GRADIENT:
            case ALGO_BELT:
            case ALGO_FIRE:
                period = (SPEED_MAX_VAL - cfg.speed) * (SPEED_MAX_VAL - cfg.speed);
                break;

            case ALGO_CLOCK2:
                period = PERIOD_SECOND * 60 / cfg.num_leds;
                break;

            default: 
                period = PERIOD_SECOND; 
                break;
        }

        TickType_t diff = xTaskGetTickCount() - lastWakeTime;
        while(pdMS_TO_TICKS(period) > diff)
        {
            if(fade_in < cfg.fadein_ms)
                transmit();
            else if(pdMS_TO_TICKS(period) > diff)
                vTaskDelay(pdMS_TO_TICKS(period) - diff);

            fade_in = pdTICKS_TO_MS(lastWakeTime - startTime);
            diff = xTaskGetTickCount() - lastWakeTime;
        }
    }
}

void Ledstrip::switchNow()
{
    cfg.power = true;
    rmt->lock(PERIOD_SECOND);
    xTaskAbortDelay( mainTask );
    rmt->unlock();
}

void Ledstrip::onoff()
{
    cfg.power = !cfg.power;
    // wait until current LED switching is finished
    rmt->lock(PERIOD_SECOND);
    xTaskAbortDelay( mainTask );
    rmt->unlock();
    startTime = xTaskGetTickCount();

    // if we just waited until rmt->transmit finished, switch off now
    if(!cfg.power)
        switchLeds();
}
