#ifndef FIXED_APP_H
#define FIXED_APP_H

#include <contiki.h>

#define TOGGLE_INTERVAL 10


#define APP_DATA_LEN_MIN                1
#define APP_DATA_LEN_MAX                6
#define APP_DATA_RAND_RANGE             (APP_DATA_LEN_MAX - APP_DATA_LEN_MIN)
#define APP_DATA_LEN_MULTIPLIER         10
// #endif

#define APP_DATA_BUFFER_SIZE 64

// #define APP_DATA_SIZES [10, 20, 30, 40, 50, 60]

void app_trafic_generator_init(void);

#endif