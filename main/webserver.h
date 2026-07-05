#pragma once
#include "esp_http_server.h"
#include "Ledstrip.h"
#include "RmtTxDriver.h"
#include <string.h>

using namespace std;

#define NR_LEDSTRIPS    CONFIG_NR_LEDSTRIPS

typedef enum {
    URI_END = 0,
    URI_SPEED,
    URI_LED,
    URI_VALUES,
    URI_SET,
    URI_POWER,
    URI_STRIPS,
    URI_GETWIFI,
    URI_SETWIFI,
} websvr_uri_t;

typedef struct {
    websvr_uri_t type;
    string uri;
    esp_err_t (*handler)(httpd_req_t *req);
} websvr_table_t;


class Webserver {
    httpd_handle_t server;
    Ledstrip ledstrip[NR_LEDSTRIPS];
    static const websvr_table_t websvr_table[];
    RmtTxDriver rmt;

    uint32_t loop_delay;
    uint32_t stripnr;
    int colorcnt;

    bool parse_stripnr(httpd_req_t *req);

public:
    Webserver();
    ~Webserver();

    esp_err_t init_leds(const char *spiffs_path);
    esp_err_t start(const char *spiffs_path);
    esp_err_t stop();
    static size_t urlDecode(const char* str, char* result, size_t resultlen);
    static bool query_key_nr(httpd_req_t *req, const char *key, unsigned long *nr);
    static bool query_key_str(httpd_req_t *req, const char *key, char *str, size_t strlen);
    esp_err_t led_get_handler(httpd_req_t *req);
    esp_err_t led_set_handler(httpd_req_t *req);
    esp_err_t get_wifi_handler(httpd_req_t *req);
    esp_err_t set_wifi_handler(httpd_req_t *req);
    esp_err_t led_val_handler(httpd_req_t *req);
    esp_err_t led_power_handler(httpd_req_t *req);
    esp_err_t led_strip_handler(httpd_req_t *req);

    static string to_json(const string& tag, uint32_t nr);
    static string to_json(const string& tag, const string& str);
};
