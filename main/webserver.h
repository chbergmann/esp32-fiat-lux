#pragma once
#include "esp_http_server.h"
#include "Ledstrip.h"
#include "RmtTxDriver.h"
#include <string.h>

using namespace std;

#define NR_LEDSTRIPS    CONFIG_NR_LEDSTRIPS

enum {
    URI_MONO = 0,
    URI_RAINBOWCLK,
    URI_RAINBOW,
    URI_SPEED,
    URI_LED,
    URI_VALUES,
    URI_SET,
    URI_WALK,
    URI_CLOCK2,
    URI_POWER,
    URI_GRADIENT,
    URI_STRIPS,
    NUM_HANDLERS
};


class Webserver {
    httpd_handle_t server;
    Ledstrip ledstrip[NR_LEDSTRIPS];
    httpd_uri_t handlers[NUM_HANDLERS];
    RmtTxDriver rmt;

    uint32_t loop_delay;
    uint32_t stripnr;
    int colorcnt;

    bool parse_stripnr(httpd_req_t *req);

public:
    Webserver();
    ~Webserver();

    esp_err_t init_leds();
    esp_err_t start();
    esp_err_t stop();
    esp_err_t led_get_handler(httpd_req_t *req);
    esp_err_t led_set_handler(httpd_req_t *req);
    esp_err_t led_val_handler(httpd_req_t *req);
    esp_err_t led_power_handler(httpd_req_t *req);
    esp_err_t led_strip_handler(httpd_req_t *req);
};
