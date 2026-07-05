
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "wifi.h"
#include "sntp.h"
#include "webserver.h"
#include "file_server.h"

static const char *TAG = "main";
static const char* spiffs_path = "/data";
static EventGroupHandle_t s_wifi_event_group;

extern "C" void app_main(void)
{
    Webserver webserver;
    bool softap_mode = false;
    bool connected = false;

    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {
        /* If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
         * and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module. */
        esp_log_level_set(TAG, (esp_log_level_t)CONFIG_LOG_MAXIMUM_LEVEL);
    }
    ESP_LOGI(TAG, "Fiat Lux!");

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default() );

    ESP_ERROR_CHECK(mount_storage(spiffs_path));
    ESP_ERROR_CHECK(webserver.init_leds(spiffs_path));
    ESP_ERROR_CHECK(webserver.start(spiffs_path));

    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(wifi_init(&s_wifi_event_group, spiffs_path));

    while(true)
    {
        if(!connected)
        {
            struct wifi_config_file_t wificfg;
            if(wifi_read_config(&wificfg) != ESP_OK)
                softap_mode = true;
            else
                softap_mode = wificfg.use_ap;

            if(softap_mode) {
                wifi_start_softap();
            } else {
                esp_err_t ret = wifi_start_sta(&wificfg);
                if(ret != ESP_OK) {
                    wifi_start_softap();
                }
            } 
        }

        EventBits_t bits = wifi_wait_for_event(s_wifi_event_group);
        ESP_LOGI(TAG, "event %X", bits);
        xEventGroupClearBits(s_wifi_event_group, bits);
            /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
        * happened. */
        if (bits & ((1 << WIFI_EVENT_STA_CONNECTED) | (1 << WIFI_EVENT_AP_STACONNECTED)))
        {
            sntp_start();
            connected = true;        
        } else if (bits & (1 << WIFI_EVENT_STA_DISCONNECTED)) {
            ESP_LOGE(TAG, "Disconnected.");
            softap_mode = true;
            connected = false;
        } else if (bits & (1 << WIFI_EVENT_AP_STADISCONNECTED)) {
            ESP_LOGE(TAG, "Disconnected from %s", CONFIG_WIFI_AP_SSID);
            connected = false;
        } else {
            ESP_LOGE(TAG, "UNEXPECTED EVENT");
        }
    }
}
