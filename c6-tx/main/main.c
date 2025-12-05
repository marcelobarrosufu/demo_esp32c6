/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"

#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "ieee802154.h"

static const char *TAG = "APP";

static void app_initialize_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

static void app_tx(void *pvParameters)
{
    ieee802154_frame_t msg;
    uint32_t cnt = 0;

    ESP_LOGI(TAG,"INIT");

    ieee802154_init();
    ieee802154_set_tx_pwr(10);
    ieee802154_set_channel(26);
    ieee802154_set_panid(26);
    ieee802154_set_short_addr(0xDEAD);

    while(true)
    {
        msg.dst_addr = 0x000B;
        msg.len = sprintf((char *)msg.data,"Sending msg %" PRIu32 " ...",cnt);

        if(ieee802154_send_msg(&msg,false))
        {
            ESP_LOGI(TAG,"Sent %" PRIu32 " ...",cnt);
        }
        else
        {
            ESP_LOGI(TAG,"Not sent %" PRIu32 " ...",cnt);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
        cnt++;
    }
}

void app_main(void)
{
    app_initialize_nvs();

    xTaskCreate(app_tx,"app_tx",2048,NULL,5,NULL);

}
