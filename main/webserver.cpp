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
#include <iostream>
#include <cctype>
#include <sstream>
#include <iomanip>
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


static esp_err_t c_led_get_handler(httpd_req_t *req);
static esp_err_t c_led_set_handler(httpd_req_t *req);
static esp_err_t c_led_val_handler(httpd_req_t *req);
static esp_err_t c_led_power_handler(httpd_req_t *req);
static esp_err_t c_led_strip_handler(httpd_req_t *req);

const websvr_table_t Webserver::websvr_table[] = {
    { URI_SPEED,  "/speed",     c_led_get_handler },
    { URI_LED,    "/led",       c_led_get_handler },
    { URI_VALUES, "/values",    c_led_val_handler },
    { URI_SET,    "/set",       c_led_set_handler },
    { URI_POWER,  "/power",     c_led_power_handler },
    { URI_STRIPS, "/strips",    c_led_strip_handler },
    { URI_END,    "",           nullptr },
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
    bool changed = false;
    uint32_t nr = 0;
    if (query_key_nr(req, "strip", &nr)) {
        if(nr < NR_LEDSTRIPS && stripnr != nr) {
            stripnr = nr;
            changed = true;
        }
    }
    return changed;
}

/* An HTTP GET handler */
esp_err_t Webserver::led_get_handler(httpd_req_t *req)
{
    bool strip_changed = parse_stripnr(req);
    led_config_t* cfg = &ledstrip[stripnr].cfg;

    bool bright_changed = false;
    /* Get value of expected key from query string */
    uint32_t bright = 0;
    if (query_key_nr(req, "bright", &bright)) {
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
        
        uint32_t val = 0;
        if (query_key_nr(req, "red", &val)) {
            color->red = (uint8_t)val;
        }
        if (query_key_nr(req, "green", &val)) {
            color->green = (uint8_t)val;
        }
        if (query_key_nr(req, "blue", &val)) {
            color->blue = (uint8_t)val;
        }
    }
    query_key_nr(req, "speed", &cfg->speed);

    for(int i=0; Ledstrip::ledfunc_table[i].algo; i++)
    {
        if(string(req->uri).find(Ledstrip::ledfunc_table[i].uri) != string::npos)
        {
            cfg->algorithm = Ledstrip::ledfunc_table[i].algo;
            if(cfg->algorithm == ALGO_GRADIENT || cfg->algorithm == ALGO_WALK)
            {
                ledstrip[stripnr].dark();
                ledstrip[stripnr].firstled(cfg->color1);
            }
        }
    }

    for(int i=0; websvr_table[i].type; i++)
    {
        if(string(req->uri).find(websvr_table[i].uri) != string::npos)
        { 
            if(websvr_table[i].type == URI_LED && cfg->algorithm == ALGO_WALK)
            {
                ledstrip[stripnr].firstled(cfg->color1);
            }
            else if(websvr_table[i].type == URI_LED && cfg->algorithm == ALGO_GRADIENT)
            {
                ledstrip[stripnr].add_gradient(cfg->color1);
            }
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
    parse_stripnr(req);
    led_config_t* cfg = &ledstrip[stripnr].cfg;

    /* Get value of expected key from query string */
    query_key_nr(req, "nr_leds", &cfg->num_leds);
    query_key_nr(req, "led1", &cfg->led1);
    query_key_nr(req, "fadein", &cfg->fadein_ms);

    char side[10] = { 0 };
    query_key_str(req, "rotate", side, sizeof(side));
    if (query_key_str(req, "rotate", side, sizeof(side))) {
        cfg->counterclock = string(side) == "left";
    }
    query_key_str(req, "stripname", cfg->name, sizeof(cfg->name));

    ledstrip[stripnr].saveConfig();

    string redirect = "<meta http-equiv=\"refresh\" content=\"0; url=/index.html\" />";
    httpd_resp_send(req, redirect.c_str(), redirect.length());
    return ESP_OK;
}


esp_err_t Webserver::led_val_handler(httpd_req_t *req)
{
    parse_stripnr(req);
    string json = ledstrip[stripnr].to_json(ledstrip[stripnr].cfg);
    
    httpd_resp_set_type(req, "application/json;charset=utf-8");
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
    httpd_resp_set_type(req, "application/json;charset=utf-8");
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
    config.max_uri_handlers = 1; 
    httpd_uri_t handler;

    handler.method = HTTP_GET;
    handler.user_ctx  = this;

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

    for(int i=0; Ledstrip::ledfunc_table[i].algo; i++)
        config.max_uri_handlers++;

    for(int i=0; websvr_table[i].type; i++)
        config.max_uri_handlers++;
        
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering %d URI handlers", config.max_uri_handlers);
        
        for(int i=0; Ledstrip::ledfunc_table[i].algo; i++)
        {
            handler.handler   = c_led_get_handler;
            handler.uri = Ledstrip::ledfunc_table[i].uri.c_str();
            httpd_register_uri_handler(server, &handler);
        }
        for(int i=0; websvr_table[i].type; i++)
        {
            handler.uri = websvr_table[i].uri.c_str();
            handler.handler = websvr_table[i].handler;
            httpd_register_uri_handler(server, &handler);
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

// Funktion zur Dekodierung von URL-kodiertem Text (char*)
size_t Webserver::urlDecode(const char* str, char* result, size_t resultlen) 
{
    if (str == NULL) {
        return 0;
    }

    // Berechne die maximale Länge des dekodierten Strings
    size_t len = strlen(str);
    if (result == NULL) {
        return 0; // Speicherallokation fehlgeschlagen
    }

    size_t resultIndex = 0;
    for (size_t i = 0; i < len; ++i) {
        if (str[i] == '%') {
            // Prüfe, ob genug Zeichen für eine hexadezimale Kodierung vorhanden sind
            if (i + 2 >= len) {
                // Ungültiges Format, breche ab oder behandle als Fehler
                result[resultIndex++] = str[i];
                continue;
            }

            // Extrahiere die hexadezimalen Zeichen
            char hexStr[] = { str[i + 1], str[i + 2], '\0' };
            int decodedChar;
            sscanf(hexStr, "%2x", &decodedChar);

            // Füge das dekodierte Zeichen zum Ergebnis hinzu
            result[resultIndex++] = (char)decodedChar;
            i += 2; // Überspringe die nächsten beiden Zeichen
        } else if (str[i] == '+') {
            // '+' wird in URL-kodierten Texten als Leerzeichen interpretiert
            result[resultIndex++] = ' ';
        } else {
            // Normale Zeichen werden unverändert übernommen
            result[resultIndex++] = str[i];
        }
        if(resultIndex >= resultlen - 1)
            break;
    }

    result[resultIndex] = '\0'; // Null-Terminator setzen
    return resultIndex;
}

bool Webserver::query_key_nr(httpd_req_t *req, const char *key, unsigned long *nr)
{
    char val[16];
    if(!query_key_str(req, key, val, sizeof(val)))
        return false;

    *nr = strtoul(val, NULL, 10);
    return true;
}

bool Webserver::query_key_str(httpd_req_t *req, const char *key, char *str, size_t strlen)
{
    size_t buf_len = httpd_req_get_url_query_len(req);
    if (buf_len <= 0) 
        return false;
    
    char buf[buf_len+1];
    char val[buf_len+1] = { 0 };
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) != ESP_OK) 
        return false;

    if (httpd_query_key_value(buf, key, val, sizeof(val)) != ESP_OK) 
        return false;

    urlDecode(val, str, strlen);
    return true;
}
#endif // !CONFIG_IDF_TARGET_LINUX
