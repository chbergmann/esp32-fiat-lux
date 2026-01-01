#include "esp_http_server.h"
#include "Ledstrip.h"
#include <string.h>

using namespace std;

enum {
    URI_MONO = 0,
    URI_RAINBOW,
    URI_SPEED,
    URI_LED,
    URI_VALUES,
    URI_SET,
    NUM_HANDLERS
};

class Webserver {
    httpd_handle_t server;
    Ledstrip ledstrip;
    httpd_uri_t handlers[NUM_HANDLERS];

    uint32_t loop_delay;
    const char* spiffs_path;

public:
    Webserver(const char* spiffs_path);
    ~Webserver();

    esp_err_t start();
    esp_err_t stop();
    esp_err_t led_get_handler(httpd_req_t *req);
    esp_err_t led_set_handler(httpd_req_t *req);
    esp_err_t led_val_handler(httpd_req_t *req);
    httpd_handle_t get_server() { return server; }
    void loop();
};
