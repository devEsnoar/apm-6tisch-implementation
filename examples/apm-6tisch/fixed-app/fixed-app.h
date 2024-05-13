#ifndef FIXED_APP_H
#define FIXED_APP_H

#include <contiki.h>

#define TOGGLE_INTERVAL 10


#define APP_DATA_LEN_MIN                7
#define APP_DATA_LEN_MAX                11
#define APP_DATA_RAND_RANGE             (APP_DATA_LEN_MAX - APP_DATA_LEN_MIN)
#define APP_DATA_LEN_MULTIPLIER         4
// #endif

#define APP_DATA_BUFFER_SIZE 64

void app_trafic_generator_init(void);

#endif