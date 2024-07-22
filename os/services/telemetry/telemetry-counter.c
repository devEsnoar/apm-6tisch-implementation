#include "telemetry-counter.h"

int telemetry_counter;

void increment_counter(int increment){
    telemetry_counter += increment;
}

void reset_counter(void){
    telemetry_counter = 0;
}

int get_counter(void){
    return telemetry_counter;
}