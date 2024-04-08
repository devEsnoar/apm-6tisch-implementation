#ifndef _INT_H_
#define _INT_H_

#include "net/mac/mac.h"
#include "net/linkaddr.h"

int inband_network_telemetry_output(void);

void inband_network_telemetry_input(void);

void inband_network_telemetry_init(void);
#endif