#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

void wifi_init_softap(EventGroupHandle_t* event_handle);
esp_err_t wifi_init_sta(const char* spiffs_path, EventGroupHandle_t* event_handle);
EventBits_t wifi_wait_for_event(EventGroupHandle_t s_wifi_event_group);
esp_err_t wifi_write_config(uint8_t ssid[32], uint8_t pwd[64]);

#ifdef __cplusplus
}
#endif