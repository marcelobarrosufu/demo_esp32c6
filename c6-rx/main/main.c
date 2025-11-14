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

void app_main(void)
{
    ieee802154_frame_t msg;

    app_initialize_nvs();

    ieee802154_init();
    ieee802154_set_tx_pwr(10);
    ieee802154_set_channel(26);
    ieee802154_set_panid(26);
    ieee802154_set_short_addr(0x000B);

    while(true)
    {
        if(ieee802154_recv_msg(&msg,500))
        {
            ESP_LOGI(TAG,"From 0x%04x '%s'",msg.src_addr,(char *)msg.data);
        }
        else
        {
            ESP_LOGI(TAG,"No message");
        }
    }

}
