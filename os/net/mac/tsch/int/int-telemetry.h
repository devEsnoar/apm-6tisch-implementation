#ifndef _INT_TELEMETRY_H_
#define _INT_TELEMETRY_H_

#include <stdint.h>
#include "int-conf.h"

/* Telemetry based on Kagaraac paper */
/*

*/


#if INT_TELEMETRY_EXPERIMENT
#define TELEMETRY_MODEL_SIZE INT_TELEMETRY_EXPERIMENT_SIZE

struct telemetry_model {
    uint8_t dummy_data[TELEMETRY_MODEL_SIZE];
};


#else
struct telemetry_model {
    uint16_t node_id;
    uint16_t channel_and_timestamp;
    uint8_t rssi;
};
#define TELEMETRY_MODEL_SIZE 5
#endif


struct int_telemetry {
    struct int_telemetry *next;
    struct telemetry_model telemetry_data;
};


void telemetry_init(void);

int input_save_telemetry_data(const uint8_t *buf, struct telemetry_model * telemetry_entry);

int create_telemetry_entry(struct telemetry_model * telemetry_entry, uint8_t bitmap);

int app_get_last_telemetry_entry(struct telemetry_model * tm_data);

#endif