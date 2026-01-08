#include "RmtTxDriver.h"
#include <cstring>
#include "led_strip_encoder.h"
#include "esp_log.h"

static const char *TAG = "RmtTxDriver";

#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)

RmtTxDriver::RmtTxDriver()
{
    led_chan = NULL;
    led_encoder = NULL;
    memset(&tx_chan_config, 0, sizeof(tx_chan_config));
    memset(&tx_config, 0, sizeof(tx_config));
    tx_chan_config.clk_src = RMT_CLK_SRC_DEFAULT; // select source clock
    tx_chan_config.mem_block_symbols = 64; // increase the block size can make the LED less flickering
    tx_chan_config.resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ;
    tx_chan_config.trans_queue_depth = 4; // set the number of transactions that can be pending in the background
    tx_config.loop_count = 0; // no transfer loop
    mutex = xSemaphoreCreateMutex();

}

RmtTxDriver::~RmtTxDriver()
{
    vSemaphoreDelete(mutex);
}

esp_err_t RmtTxDriver::init(gpio_num_t gpionr)
{
    tx_chan_config.gpio_num = gpionr;
    ESP_LOGI(TAG, "Create RMT TX channel");
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, &led_chan));

    ESP_LOGI(TAG, "Install led strip encoder");
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_LED_STRIP_RESOLUTION_HZ,
    };
    ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, &led_encoder));
    return rmt_enable(led_chan);
}

esp_err_t RmtTxDriver::transmit(uint8_t* pixels, size_t nr_pixels, int timeout_ms)
{
    esp_err_t ret = rmt_transmit(led_chan, led_encoder, pixels, nr_pixels, &tx_config);
    if(ret == ESP_OK)
        ret = rmt_tx_wait_all_done(led_chan, timeout_ms);

    return ret;
}

esp_err_t RmtTxDriver::transmit(gpio_num_t gpionr, uint8_t *pixels, size_t nr_pixels, int timeout_ms)
{
    if(!xSemaphoreTake(mutex, pdMS_TO_TICKS(timeout_ms)))
        return ESP_ERR_TIMEOUT;

    if(gpionr != tx_chan_config.gpio_num)
    {
        ESP_ERROR_CHECK(rmt_disable(led_chan));
        rmt_tx_switch_gpio(led_chan, gpionr, false);
        tx_chan_config.gpio_num = gpionr;
        ESP_ERROR_CHECK(rmt_enable(led_chan));
    }

    esp_err_t ret = transmit(pixels, nr_pixels, timeout_ms);

    xSemaphoreGive(mutex);
    return ret;
}
