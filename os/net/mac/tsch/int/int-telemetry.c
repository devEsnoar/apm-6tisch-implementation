#include "int-telemetry.h"
#include "net/netstack.h"
#include "sys/log.h"
#include "lib/list.h"
#include "sys/node-id.h"
#include "tsch/tsch-asn.h"
#include "dev/radio.h"
#define LOG_MODULE "INT"
#define LOG_LEVEL LOG_LEVEL_INT

#include "lib/memb.h"

extern int is_source_appdata;
extern struct tsch_asn_t tsch_current_asn;
extern uint8_t tsch_current_channel;

MEMB(app_telemetry_memb, struct int_telemetry, 256);

LIST(app_telemetry_list);

void telemetry_init(void){
    memb_init(&app_telemetry_memb);
    list_init(app_telemetry_list);
}

int input_save_telemetry_data(const uint8_t *buf, struct telemetry_model * telemetry_entry) {

    telemetry_entry->node_id = buf[0] + (buf[1] << 8);
    uint8_t channel = buf[3] >> 4;
    telemetry_entry->channel_and_timestamp = (!channel) ? (tsch_current_channel << 12) + (buf[2] + (buf[3] << 8)) :  buf[2] + (buf[3] << 8);
    
    telemetry_entry->rssi = buf[4];
    LOG_INFO("INT Telemetry: tm_entry_app Node ID: %d, Channel and Timestamp: %d, %d RSSI: %d\n", 
        telemetry_entry->node_id, 
        telemetry_entry->channel_and_timestamp >> 12, 
        telemetry_entry->channel_and_timestamp & 0x0FFF, 
        telemetry_entry->rssi
    );

    struct int_telemetry * int_telemetry_app = memb_alloc(&app_telemetry_memb);

    if(int_telemetry_app != NULL) {
        struct telemetry_model * tm_entry_app = &int_telemetry_app->telemetry_data;
        memset(tm_entry_app, 0, sizeof(struct telemetry_model));
        // LOG_DBG("INT Telemetry: telemetry_entry Node ID: %d, Channel and Timestamp: %d, RSSI: %d\n", telemetry_entry->node_id, telemetry_entry->channel_and_timestamp, telemetry_entry->rssi);
        // LOG_INFO("INT Telemetry: tm_entry_app Node ID: %d, Channel and Timestamp: %d, RSSI: %d\n", tm_entry_app->node_id, tm_entry_app->channel_and_timestamp, tm_entry_app->rssi);
        // memcpy(tm_entry_app, telemetry_entry, sizeof(struct telemetry_model));
        *tm_entry_app = *telemetry_entry;
        list_push(app_telemetry_list, int_telemetry_app);
        LOG_DBG("INT Telemetry: Telemetry entry saved for app consumption\n");
    }
    else {
        LOG_WARN("INT Telemetry: Telemetry entries memory is full, app is not consuming list\n");
    }


    return 0;
}

int create_telemetry_entry(struct telemetry_model * telemetry_entry, uint8_t bitmap){
    // TODO: Implement bitmap verification
    radio_value_t radio_last_rssi;
    if(bitmap){
        telemetry_entry->node_id = node_id;
        if(is_source_appdata) {
            clock_time_t now = clock_time();
            telemetry_entry->channel_and_timestamp = (0x0 << 12) + (uint16_t) (now & (uint32_t) 0x0FFF);
            telemetry_entry->rssi = 0;
        }        
        else {
            telemetry_entry->channel_and_timestamp = (0x0 << 12) + (uint16_t) (tsch_current_asn.ls4b & (uint32_t) 0x0FFF);
            NETSTACK_RADIO.get_value(RADIO_PARAM_LAST_RSSI, &radio_last_rssi);
            telemetry_entry->rssi = radio_last_rssi;
        }
        
        LOG_INFO("INT Telemetry: Creating telemetry entry Node ID: %d, Channel and Timestamp: %d, %d RSSI: %d\n", 
            telemetry_entry->node_id, 
            telemetry_entry->channel_and_timestamp >> 12, 
            telemetry_entry->channel_and_timestamp & 0x0FFF, 
            (uint8_t) telemetry_entry->rssi
        );
    }

    return 0;

}

int app_get_last_telemetry_entry(struct telemetry_model * tm_data){
    struct int_telemetry * int_telemetry_app = list_chop(app_telemetry_list);
    if(int_telemetry_app != NULL) {
        *tm_data = int_telemetry_app->telemetry_data;
        memb_free(&app_telemetry_memb, int_telemetry_app);
        return 0;
    }
    else {
        return 1;
    }
}