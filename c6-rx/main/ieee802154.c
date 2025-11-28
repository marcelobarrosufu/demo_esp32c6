
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "esp_system.h"
#include "esp_log.h"
#include "soc/soc.h"
#include "esp_ieee802154.h"
#include "soc/ieee802154_reg.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "ieee802154.h"

static const char *TAG = "IEEE802154";

#define RX_QUEUE_LENGTH 5

typedef struct ieee802154_ctrl_s
{
    uint8_t channel;
    uint16_t panid;
    uint16_t short_addr;
    bool enabled;
    uint8_t seq_num;
    uint8_t tx[128];
    int8_t tx_pwr;
    SemaphoreHandle_t tx_sem;
    volatile bool tx_ok;
    uint32_t tx_timeout_ms;
    QueueHandle_t rx_queue;
} ieee802154_ctrl_t;

static ieee802154_ctrl_t ieee802154_ctrl = 
{
    .channel = 26,
    .panid = 26,
    .short_addr = 0,
    .enabled = false,
    .seq_num = 0,
    .tx_pwr = 20,
    .tx_timeout_ms = 200,
    .tx_ok = false,
    .tx_sem = NULL,
};

bool ieee802154_check_if_enabled(void)
{
    if (!ieee802154_ctrl.enabled) 
    {
        ESP_LOGI(TAG,"IEEE 802.15.4 is not initialized");
    }

    return ieee802154_ctrl.enabled;
}

bool ieee802154_init(void) 
{
    if(!ieee802154_ctrl.enabled) 
    {
        ieee802154_ctrl.tx_sem = xSemaphoreCreateBinary();
        if(ieee802154_ctrl.tx_sem) 
        {
            ieee802154_ctrl.rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(ieee802154_frame_t));
            if(ieee802154_ctrl.rx_queue)
            {
                if(esp_ieee802154_enable() == ESP_OK)
                {
                    //esp_ieee802154_set_cca_threshold(-94);
                    esp_ieee802154_set_ack_timeout(120*16);
                    esp_ieee802154_set_promiscuous(false);
                    esp_ieee802154_set_rx_when_idle(true);
                    esp_ieee802154_set_coordinator(false);
                    esp_ieee802154_receive();                
                    ieee802154_ctrl.enabled = true;
                }
                else
                {
                    vSemaphoreDelete(ieee802154_ctrl.tx_sem);
                    vQueueDelete(ieee802154_ctrl.rx_queue);
                    ESP_LOGE(TAG,"Failed to enable IEEE 802.15.4");
                }
            }
            else
            {
                vSemaphoreDelete(ieee802154_ctrl.tx_sem);
                ESP_LOGE(TAG,"Failed to create queue");
            }
        }
        else
        {
            ESP_LOGE(TAG,"Failed to create TX semaphore");
        }
    }

    return ieee802154_ctrl.enabled ;
}

bool ieee802154_deinit(void) 
{
    if (ieee802154_ctrl.enabled) 
    {
        if(esp_ieee802154_disable() == ESP_OK)
        {
            vSemaphoreDelete(ieee802154_ctrl.tx_sem);
            vQueueDelete(ieee802154_ctrl.rx_queue);

            ieee802154_ctrl.enabled = false;
        }
        else
        {
            ESP_LOGE(TAG,"Failed to disable IEEE 802.15.4");
        }
    }

    return (ieee802154_ctrl.enabled == false);
}

bool ieee802154_set_tx_pwr(int8_t pwr)
{
    if(!ieee802154_check_if_enabled())
        return false;

    if (pwr < -15 || pwr > 20) 
    {
        ESP_LOGE(TAG,"TX Power must be between -15 and 20 dBm");
        return false;
    }

    if(esp_ieee802154_set_txpower(pwr) == ESP_OK)
    {
        ieee802154_ctrl.tx_pwr = pwr;
    }
    else
    {
        ESP_LOGE(TAG,"Failed to set TX Power");
        return false;
    }

    return true;
}

bool ieee802154_set_channel(uint8_t channel) 
{
    if(!ieee802154_check_if_enabled())
        return false;

    if (channel < 11 || channel > 26) 
    {
        ESP_LOGE(TAG,"Channel must be between 11 and 26");
        return false;
    }

    esp_err_t ret = esp_ieee802154_set_channel(channel);
    if (ret == ESP_OK)
    {
        ieee802154_ctrl.channel = channel;
    } 
    else
    {
        ESP_LOGE(TAG,"Failed to set channel");
        return false;
    }

    return true;
}

uint8_t ieee802154_get_channel(void) 
{
    if(!ieee802154_check_if_enabled())
        return 0;

    uint8_t channel = esp_ieee802154_get_channel();

    return channel;
}

bool ieee802154_set_panid(uint16_t panid) 
{
    if(!ieee802154_check_if_enabled())
        return false;

    esp_err_t ret = esp_ieee802154_set_panid(panid);
    if (ret == ESP_OK) 
    {
        ieee802154_ctrl.panid = panid;
    } 
    else
    {
        ESP_LOGE(TAG,"Failed to set PANID");
        return false;
    }

    return true;
}

uint16_t ieee802154_get_panid(void) 
{
    if(!ieee802154_check_if_enabled())
        return 0;

    uint16_t panid = esp_ieee802154_get_panid();

    return panid;
}

bool ieee802154_set_short_addr(uint16_t short_addr) 
{
    if(!ieee802154_check_if_enabled())
        return false;

    esp_err_t ret = esp_ieee802154_set_short_address(short_addr);
    if (ret == ESP_OK) 
    {
        ieee802154_ctrl.short_addr = (uint16_t)short_addr;
    } 
    else
    {
        ESP_LOGE(TAG,"Failed to set short address");
        return false;
    }

    return true;
}

uint16_t ieee802154_get_short_addr(void) 
{
    if(!ieee802154_check_if_enabled())
        return 0;

    uint16_t short_addr = esp_ieee802154_get_short_address();

    return short_addr;
}

static uint8_t ieee802154_get_next_seq_number(bool retry)
{
    if (!retry) 
    {
        ieee802154_ctrl.seq_num++;
    }
    return ieee802154_ctrl.seq_num;
}

static bool ieee802154_frame_filter(uint8_t *data, esp_ieee802154_frame_info_t *frame_info, ieee802154_frame_t *msg)
{
    uint8_t len = data[0];

    if(len < 11)
        return false;

    // XCXX XTTT 41
    // SSXXDDXX. 88
    // filter: 
    // - pand id compression
    // - only compressed address source and destination addresses
    // - only to this node or broadcast
    // - same pan id
    // - only data frames
    if(((data[1] & 0x43) == 0x41) && // Data Frame, compressed PAN ID
       ((data[2] & 0xCC) == 0x88)) // Short Address for both source and dest
    {
        // seq number data[3]
        uint16_t pan_id = data[4] | (data[5] << 8);
        if(pan_id != ieee802154_ctrl.panid)
            return false;

        uint16_t dst_addr = data[6] | (data[7] << 8);
        if(dst_addr != ieee802154_ctrl.short_addr && dst_addr != 0xFFFF)
            return false;

        uint16_t src_addr = data[8] | (data[9] << 8);

        msg->src_addr = src_addr;
        msg->dst_addr = dst_addr;
        msg->seq_num = data[3];
        msg->len = len - 11;

        memcpy(msg->data, &data[10], msg->len);

        return true;
    }

    return false;
}
void esp_ieee802154_receive_done(uint8_t *data, esp_ieee802154_frame_info_t *frame_info)
{
    ieee802154_frame_t msg;

    if(ieee802154_frame_filter(data, frame_info, &msg))
    {
        if(xQueueSendToBackFromISR(ieee802154_ctrl.rx_queue, &msg, NULL) != pdPASS)
        {
            ESP_DRAM_LOGI(TAG,"Queue full !");
        }
    }

    esp_ieee802154_receive_handle_done(data);
}

bool ieee802154_recv_msg(ieee802154_frame_t *msg, uint32_t timeout_ms)
{
    if(!ieee802154_check_if_enabled())
        return false;

    esp_ieee802154_receive();

    if (xQueueReceive(ieee802154_ctrl.rx_queue, msg, pdMS_TO_TICKS(timeout_ms)) != pdPASS)
    {
        //ESP_LOGE(TAG,"Queue empty");
        return false;
    }

    return true;
}

static void ieee802154_set_tx_status(bool in_progress)
{
    ieee802154_ctrl.tx_ok = in_progress;
}

static bool ieee802154_get_tx_status(void)
{
    return ieee802154_ctrl.tx_ok;
}

void esp_ieee802154_transmit_done(const uint8_t *frame, const uint8_t *ack, esp_ieee802154_frame_info_t *ack_frame_info)
{
    ESP_DRAM_LOGI(TAG,"TX done %c",(*ack?'A':' '));

    if(ack)
    {
        esp_ieee802154_receive_handle_done(frame);
    }
    else
    {
        ieee802154_set_tx_status(true);
        xSemaphoreGiveFromISR(ieee802154_ctrl.tx_sem, NULL);
    }
}

void esp_ieee802154_transmit_failed(const uint8_t *frame, esp_ieee802154_tx_error_t error)
{
    ieee802154_set_tx_status(false);
    esp_ieee802154_state_t s = esp_ieee802154_get_state();
    uint8_t err[] = {'0', '1', '2', '3', '4', '5'}; 
    ESP_DRAM_LOGI(TAG,"TX error %c %d",err[error],s);
    xSemaphoreGiveFromISR(ieee802154_ctrl.tx_sem, NULL);
}

bool ieee802154_send_msg(ieee802154_frame_t *msg, bool retry)
{
    if(!ieee802154_check_if_enabled())
        return false;

    if(msg->len == 0 || msg->len > 116)
    {
        ESP_LOGI(TAG,"Payload size must be between 1 and 116 bytes");
        return false;
    }

    // full size: FC(2)+SN(1)+DPANID(2)+DADDR(2)+SADDR(2) + PAYLOAD + FCS(2)
    ieee802154_ctrl.tx[0] = msg->len + 11; // FC(2)+SN(1)+DPANID(2)+DADDR(2)+SADDR(2)
    ieee802154_ctrl.tx[1] = 0x61; // Frame Control: Data Frame, Ack Request, PAN ID Compression
    ieee802154_ctrl.tx[2] = 0x88; // Frame Control: Src and Dst Addr are short (16 bit)
    ieee802154_ctrl.tx[3] = ieee802154_get_next_seq_number(retry);
    ieee802154_ctrl.tx[4] = (uint8_t)(ieee802154_ctrl.panid & 0xFF);         // Dest PAN ID LSB
    ieee802154_ctrl.tx[5] = (uint8_t)(ieee802154_ctrl.panid >> 8);           // Dest PAN ID MSB
    ieee802154_ctrl.tx[6] = (uint8_t)(msg->dst_addr & 0xFF);                      // Dest Address LSB
    ieee802154_ctrl.tx[7] = (uint8_t)(msg->dst_addr >> 8);                        // Dest Address MSB
    ieee802154_ctrl.tx[8] = (uint8_t)(ieee802154_ctrl.short_addr & 0xFF); // Src Address LSB
    ieee802154_ctrl.tx[9] = (uint8_t)(ieee802154_ctrl.short_addr >> 8);   // Src Address MSB

    memcpy(&(ieee802154_ctrl.tx[10]), msg->data, msg->len);

    ieee802154_set_tx_status(false);
    esp_err_t ret = esp_ieee802154_transmit(ieee802154_ctrl.tx, true);
    bool status = (ret == ESP_OK);

    // wait ack
    if(status)
    {
        status = false;
        if(xSemaphoreTake(ieee802154_ctrl.tx_sem, pdMS_TO_TICKS(ieee802154_ctrl.tx_timeout_ms)) == pdTRUE)
        {
             if(ieee802154_get_tx_status() == true)
             status = true;
        }
    }
    else
    {
        ESP_LOGE(TAG,"TX error: %s",esp_err_to_name(ret));
        status = false;
    }

    return status;
}
