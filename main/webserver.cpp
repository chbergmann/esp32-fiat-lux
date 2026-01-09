/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "protocol_examples_utils.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#include "esp_check.h"
#include <time.h>
#include <sys/time.h>
#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#endif  // !CONFIG_IDF_TARGET_LINUX
#include "Ledstrip.h"
#include <string>
#include "webserver.h"
#include "esp_timer.h"
#include "file_server.h"
#include "freertos/task.h"

#define EXAMPLE_HTTP_QUERY_KEY_MAX_LEN  (64)

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "webserver";

static const char* SITES[NUM_HANDLERS] = {
    "/mono",
    "/rainbowclk",
    "/rainbow",
    "/speed",
    "/led",
    "/values",
    "/set",
    "/walk",
    "/clock2",
    "/power",
    "/gradient",
    "/strips"
};

static const char* spiffs_path = "/data";

Webserver::Webserver()
{
    server = NULL;
    stripnr = 0;
    colorcnt = 0;
}

Webserver::~Webserver()
{
    stop();
}

#if CONFIG_EXAMPLE_BASIC_AUTH

typedef struct {
    char    *username;
    char    *password;
} basic_auth_info_t;

#define HTTPD_401      "401 UNAUTHORIZED"           /*!< HTTP Response 401 */

static char *http_auth_basic(const char *username, const char *password)
{
    size_t out;
    char *user_info = NULL;
    char *digest = NULL;
    size_t n = 0;
    int rc = asprintf(&user_info, "%s:%s", username, password);
    if (rc < 0) {
        ESP_LOGE(TAG, "asprintf() returned: %d", rc);
        return NULL;
    }

    if (!user_info) {
        ESP_LOGE(TAG, "No enough memory for user information");
        return NULL;
    }
    esp_crypto_base64_encode(NULL, 0, &n, (const unsigned char *)user_info, strlen(user_info));

    /* 6: The length of the "Basic " string
     * n: Number of bytes for a base64 encode format
     * 1: Number of bytes for a reserved which be used to fill zero
    */
    digest = calloc(1, 6 + n + 1);
    if (digest) {
        strcpy(digest, "Basic ");
        esp_crypto_base64_encode((unsigned char *)digest + 6, n, &out, (const unsigned char *)user_info, strlen(user_info));
    }
    free(user_info);
    return digest;
}

/* An HTTP GET handler */
static esp_err_t basic_auth_get_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = 0;
    basic_auth_info_t *basic_auth_info = req->user_ctx;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
        buf = calloc(1, buf_len);
        if (!buf) {
            ESP_LOGE(TAG, "No enough memory for basic authorization");
            return ESP_ERR_NO_MEM;
        }

        if (httpd_req_get_hdr_value_str(req, "Authorization", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Authorization: %s", buf);
        } else {
            ESP_LOGE(TAG, "No auth value received");
        }

        char *auth_credentials = http_auth_basic(basic_auth_info->username, basic_auth_info->password);
        if (!auth_credentials) {
            ESP_LOGE(TAG, "No enough memory for basic authorization credentials");
            free(buf);
            return ESP_ERR_NO_MEM;
        }

        if (strncmp(auth_credentials, buf, buf_len)) {
            ESP_LOGE(TAG, "Not authenticated");
            httpd_resp_set_status(req, HTTPD_401);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
            httpd_resp_send(req, NULL, 0);
        } else {
            ESP_LOGI(TAG, "Authenticated!");
            char *basic_auth_resp = NULL;
            httpd_resp_set_status(req, HTTPD_200);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_set_hdr(req, "Connection", "keep-alive");
            int rc = asprintf(&basic_auth_resp, "{\"authenticated\": true,\"user\": \"%s\"}", basic_auth_info->username);
            if (rc < 0) {
                ESP_LOGE(TAG, "asprintf() returned: %d", rc);
                free(auth_credentials);
                return ESP_FAIL;
            }
            if (!basic_auth_resp) {
                ESP_LOGE(TAG, "No enough memory for basic authorization response");
                free(auth_credentials);
                free(buf);
                return ESP_ERR_NO_MEM;
            }
            httpd_resp_send(req, basic_auth_resp, strlen(basic_auth_resp));
            free(basic_auth_resp);
        }
        free(auth_credentials);
        free(buf);
    } else {
        ESP_LOGE(TAG, "No auth header received");
        httpd_resp_set_status(req, HTTPD_401);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"Hello\"");
        httpd_resp_send(req, NULL, 0);
    }

    return ESP_OK;
}

static httpd_uri_t basic_auth = {
    .uri       = "/basic_auth",
    .method    = HTTP_GET,
    .handler   = basic_auth_get_handler,
};

static void httpd_register_basic_auth(httpd_handle_t server)
{
    basic_auth_info_t *basic_auth_info = calloc(1, sizeof(basic_auth_info_t));
    if (basic_auth_info) {
        basic_auth_info->username = CONFIG_EXAMPLE_BASIC_AUTH_USERNAME;
        basic_auth_info->password = CONFIG_EXAMPLE_BASIC_AUTH_PASSWORD;

        basic_auth.user_ctx = basic_auth_info;
        httpd_register_uri_handler(server, &basic_auth);
    }
}
#endif


static esp_err_t c_led_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s", req->uri);
    Webserver* webserver = (Webserver*)req->user_ctx;
    return webserver->led_get_handler(req);
}

static esp_err_t c_led_set_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s", req->uri);
    Webserver* webserver = (Webserver*)req->user_ctx;
    return webserver->led_set_handler(req);
}

static esp_err_t c_led_val_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s", req->uri);
    Webserver* webserver = (Webserver*)req->user_ctx;
    return webserver->led_val_handler(req);
}

static esp_err_t c_led_power_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s", req->uri);
    Webserver* webserver = (Webserver*)req->user_ctx;
    return webserver->led_power_handler(req);
}

static esp_err_t c_led_strip_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s", req->uri);
    Webserver* webserver = (Webserver*)req->user_ctx;
    return webserver->led_strip_handler(req);
}

bool Webserver::parse_stripnr(httpd_req_t *req)
{
    size_t buf_len;
    bool changed = false;
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char buf[buf_len];
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found URL query => %s", buf);
            char col[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            if (httpd_query_key_value(buf, "strip", col, sizeof(col)) == ESP_OK) {
                uint32_t nr = strtoul(col, NULL, 10);
                if(nr < NR_LEDSTRIPS && stripnr != nr) {
                    stripnr = nr;
                    changed = true;
                }
            }
        }
    }
    return changed;
}

/* An HTTP GET handler */
esp_err_t Webserver::led_get_handler(httpd_req_t *req)
{
    size_t buf_len;

    bool strip_changed = parse_stripnr(req);
    led_config_t* cfg = &ledstrip[stripnr].cfg;

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char buf[buf_len];
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            bool bright_changed = false;
            char col[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "bright", col, sizeof(col)) == ESP_OK) {
                uint32_t bright = strtoul(col, NULL, 10);
                if(bright != cfg->bright) {
                    cfg->bright = bright;
                    bright_changed = true;
                }
            }
            if(!bright_changed || cfg->algorithm == ALGO_MONO) {
                color_t* color = &cfg->color1;
                if(cfg->algorithm == ALGO_CLOCK2) {
                    if(colorcnt == 1) {
                        colorcnt = 0;
                        color = &cfg->color2;
                    }
                    else colorcnt++;
                }
                else colorcnt = 0;
               
                if (httpd_query_key_value(buf, "red", col, sizeof(col)) == ESP_OK) {
                    color->red = (uint8_t)strtoul(col, NULL, 10);
                }
                if (httpd_query_key_value(buf, "green", col, sizeof(col)) == ESP_OK) {
                    color->green = (uint8_t)strtoul(col, NULL, 10);
                }
                if (httpd_query_key_value(buf, "blue", col, sizeof(col)) == ESP_OK) {
                    color->blue = (uint8_t)strtoul(col, NULL, 10);
                }
            }
            if (httpd_query_key_value(buf, "speed", col, sizeof(col)) == ESP_OK) {
                cfg->speed = strtoul(col, NULL, 10);
            }
        }
    }

    if(string(req->uri).find(SITES[URI_MONO]) != string::npos)
        cfg->algorithm = ALGO_MONO;

    else if(string(req->uri).find(SITES[URI_GRADIENT]) != string::npos)
    {
        ledstrip[stripnr].dark();
        cfg->algorithm = ALGO_GRADIENT;
        ledstrip[stripnr].firstled(cfg->color1);
    }

    else if(string(req->uri).find(SITES[URI_RAINBOWCLK]) != string::npos)
        cfg->algorithm = ALGO_RAINBOWCLK;

    else if(string(req->uri).find(SITES[URI_RAINBOW]) != string::npos)
        cfg->algorithm = ALGO_RAINBOW;

    else if(string(req->uri).find(SITES[URI_WALK]) != string::npos)
    {
        ledstrip[stripnr].dark();
        cfg->algorithm = ALGO_WALK;
        ledstrip[stripnr].firstled(cfg->color1);
    }

    else if(string(req->uri).find(SITES[URI_CLOCK2]) != string::npos)
        cfg->algorithm = ALGO_CLOCK2;

    if(string(req->uri).find(SITES[URI_LED]) != string::npos)
    { 
        if(cfg->algorithm == ALGO_WALK)
        {
            ledstrip[stripnr].firstled(cfg->color1);
        }
        else if(cfg->algorithm == ALGO_GRADIENT)
        {
            ledstrip[stripnr].add_gradient(cfg->color1);
        }
    }

    ledstrip[stripnr].switchNow();
    ledstrip[stripnr].saveConfig();

    if(strip_changed) {
        string result = "RELOAD";
        httpd_resp_send(req, result.c_str(), result.length());
    }
    else {
        httpd_resp_send(req, NULL, 0);
    }
    return ESP_OK;
}


/* An HTTP GET handler */
esp_err_t Webserver::led_set_handler(httpd_req_t *req)
{
    size_t buf_len;

    parse_stripnr(req);
    led_config_t* cfg = &ledstrip[stripnr].cfg;

    /* Read URL query string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char buf[buf_len];
        ESP_RETURN_ON_FALSE(buf, ESP_ERR_NO_MEM, TAG, "buffer alloc failed");
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char col[EXAMPLE_HTTP_QUERY_KEY_MAX_LEN] = {0};
            /* Get value of expected key from query string */
            if (httpd_query_key_value(buf, "nr_leds", col, sizeof(col)) == ESP_OK) {
                cfg->num_leds = strtoul(col, NULL, 10);
            }
            if (httpd_query_key_value(buf, "rotate", col, sizeof(col)) == ESP_OK) {
                cfg->counterclock = string(col) == "left";
            }
            if (httpd_query_key_value(buf, "led1", col, sizeof(col)) == ESP_OK) {
                cfg->led1 = strtoul(col, NULL, 10);
            }
            if (httpd_query_key_value(buf, "stripname", cfg->name, sizeof(cfg->name)) == ESP_OK) {
                for(int i=0; i<sizeof(cfg->name); i++) {
                    if(cfg->name[i] == '+')
                        cfg->name[i] = ' ';
                }
            }
        }
    }

    ledstrip[stripnr].saveConfig();

    string redirect = "<meta http-equiv=\"refresh\" content=\"0; url=/index.html\" />";
    httpd_resp_send(req, redirect.c_str(), redirect.length());
    return ESP_OK;
}


esp_err_t Webserver::led_val_handler(httpd_req_t *req)
{
    parse_stripnr(req);
    string json = ledstrip[stripnr].to_json(ledstrip[stripnr].cfg);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

esp_err_t Webserver::led_power_handler(httpd_req_t *req)
{
    ledstrip[stripnr].onoff();
    ledstrip[stripnr].saveConfig();

    httpd_resp_send(req, NULL, 0);
    /* After sending the HTTP response the old HTTP request headers are lost. */
    return ESP_OK;
}

esp_err_t Webserver::led_strip_handler(httpd_req_t *req)
{
    string json = "{" + 
        Ledstrip::to_json("nr_strips", NR_LEDSTRIPS) + "," +
        Ledstrip::to_json("selected_strip", stripnr) + "," +
        "\"name\":[";

    for(int i=0; i<NR_LEDSTRIPS; i++)
    {
        if(i > 0)
            json += ",";

        json += "\"" + string(ledstrip[i].cfg.name) + "\"";
    }
    json += "]}";
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    return esp_err_t();
}

/* This handler allows the custom error handling functionality to be
 * tested from client side. For that, when a PUT request 0 is sent to
 * URI /ctrl, the /hello and /echo URIs are unregistered and following
 * custom error handler http_404_error_handler() is registered.
 * Afterwards, when /hello or /echo is requested, this custom error
 * handler is invoked which, after sending an error message to client,
 * either closes the underlying socket (when requested URI is /echo)
 * or keeps it open (when requested URI is /hello). This allows the
 * client to infer if the custom error handler is functioning as expected
 * by observing the socket state.
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    char msg[CONFIG_HTTPD_MAX_URI_LEN+17];
    snprintf(msg, sizeof(msg), "%s does not exist!", req->uri);
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, msg);
    return ESP_FAIL;
}


#if CONFIG_EXAMPLE_ENABLE_SSE_HANDLER
/* An HTTP GET handler for SSE */
static esp_err_t sse_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");

    char sse_data[64];
    while (1) {
        struct timeval tv;
        gettimeofday(&tv, NULL); // Get the current time
        int64_t time_since_boot = tv.tv_sec; // Time since boot in seconds
        esp_err_t err;
        int len = snprintf(sse_data, sizeof(sse_data), "data: Time since boot: %" PRIi64 " seconds\n\n", time_since_boot);
        if ((err = httpd_resp_send_chunk(req, sse_data, len)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send sse data (returned %02X)", err);
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); // Send data every second
    }

    httpd_resp_send_chunk(req, NULL, 0); // End response
    return ESP_OK;
}

static const httpd_uri_t sse = {
    .uri       = "/sse",
    .method    = HTTP_GET,
    .handler   = sse_handler,
    .user_ctx  = NULL
};
#endif // CONFIG_EXAMPLE_ENABLE_SSE_HANDLER

esp_err_t Webserver::init_leds()
{
    esp_err_t ret = ESP_OK;
    ESP_ERROR_CHECK(mount_storage(spiffs_path));

    rmt.init((gpio_num_t)CONFIG_LED_STRIP1_GPIO_NUM);
    for(int i=0; i<NR_LEDSTRIPS; i++)
    {
        int gpio = CONFIG_LED_STRIP1_GPIO_NUM;
        if(i > 0)
            gpio = CONFIG_LED_STRIP2_GPIO_NUM + i - 1;

        ret = ledstrip[i].init(spiffs_path, &rmt, (gpio_num_t)gpio);
        if(ret != ESP_OK)
        {
            ESP_LOGE(TAG, "failed to initialize ledstripat GPIO %d", gpio);
            break;
        }
    }
    return ret;
}

esp_err_t Webserver::start(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers   = NUM_HANDLERS + 1; 

    for(int i=0; i<NUM_HANDLERS; i++) {
        handlers[i].method = HTTP_GET;
        handlers[i].handler   = c_led_get_handler;
        handlers[i].user_ctx  = this;
        handlers[i].uri = SITES[i];
    }
    handlers[URI_VALUES].handler = c_led_val_handler;
    handlers[URI_SET].handler    = c_led_set_handler;
    handlers[URI_POWER].handler  = c_led_power_handler;
    handlers[URI_STRIPS].handler = c_led_strip_handler;

#if CONFIG_IDF_TARGET_LINUX
    // Setting port as 8001 when building for Linux. Port 80 can be used only by a privileged user in linux.
    // So when a unprivileged user tries to run the application, it throws bind error and the server is not started.
    // Port 8001 can be used by an unprivileged user as well. So the application will not throw bind error and the
    // server will be started.
    config.server_port = 8001;
#endif // !CONFIG_IDF_TARGET_LINUX
    config.lru_purge_enable = true;

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        for(int i=0; i<NUM_HANDLERS; i++) { 
            httpd_register_uri_handler(server, &handlers[i]);
        }
        ESP_ERROR_CHECK(start_file_server(server, spiffs_path));
#if CONFIG_EXAMPLE_ENABLE_SSE_HANDLER
        httpd_register_uri_handler(server, &sse); // Register SSE handler
#endif
#if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
#endif
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return ESP_FAIL;
}

#if !CONFIG_IDF_TARGET_LINUX
esp_err_t Webserver::stop()
{
    // Stop the httpd server
    esp_err_t err = httpd_stop(server);
    server = NULL;
    return err;
}

#endif // !CONFIG_IDF_TARGET_LINUX
