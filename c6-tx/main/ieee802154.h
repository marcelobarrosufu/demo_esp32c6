#pragma once

#define MAX_FRAME_LEN   127

typedef struct ieee802154_frame_s
{
    uint8_t data[MAX_FRAME_LEN];
    uint16_t len;
    uint16_t src_addr;
    uint16_t dst_addr;
    uint8_t seq_num;
} ieee802154_frame_t;

bool ieee802154_check_if_enabled(void);
bool ieee802154_init(void);
bool ieee802154_deinit(void);
bool ieee802154_set_tx_pwr(int8_t pwr);
bool ieee802154_set_channel(uint8_t channel);
uint8_t ieee802154_get_channel(void);
bool ieee802154_set_panid(uint16_t panid);
uint16_t ieee802154_get_panid(void);
bool ieee802154_set_short_addr(uint16_t short_addr);
uint16_t ieee802154_get_short_addr(void);
bool ieee802154_recv_msg(ieee802154_frame_t *msg, uint32_t timeout_ms);
bool ieee802154_send_msg(ieee802154_frame_t *msg, bool retry);
