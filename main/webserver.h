#include "esp_http_server.h"
#include "Ledstrip.h"
#include <string.h>

using namespace std;

#define NUM_HANDLERS    3

class Webserver {
    httpd_handle_t server;
    Ledstrip ledstrip;
    httpd_uri_t handlers[NUM_HANDLERS];

    uint32_t loop_delay;

public:
    Webserver();
    ~Webserver();

    esp_err_t start();
    esp_err_t stop();
    esp_err_t led_get_handler(httpd_req_t *req);
    httpd_handle_t get_server() { return server; }
    void loop();
};
