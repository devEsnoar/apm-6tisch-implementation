#ifndef _INT_H_
#define _INT_H_

#define INT_SUBIE_ID 0xCA

#include "net/mac/mac.h"
#include "net/linkaddr.h"

int inband_network_telemetry_output(void);

void inband_network_telemetry_input(void);

#endif