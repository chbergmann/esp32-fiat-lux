#include "esp_wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSID_SIZE 32
#define PWD_SIZE  64

struct wifi_config_file_t
{
    uint8_t ssid[SSID_SIZE];
    uint8_t pwd[PWD_SIZE];
    bool use_ap;
};

esp_err_t wifi_init(EventGroupHandle_t* p_wifi_event_group, const char* spiffs_path);
void wifi_start_softap();
EventBits_t wifi_wait_for_event(EventGroupHandle_t s_wifi_event_group);
esp_err_t wifi_start_sta(struct wifi_config_file_t* pCfg);

esp_err_t wifi_write_config(struct wifi_config_file_t* pCfg);
esp_err_t wifi_read_config(struct wifi_config_file_t* pCfg);

#ifdef __cplusplus
}
#endif