#ifndef _INT_ENGINE_H_
#define _INT_ENGINE_H_

#include "lib/list.h"
#include <stdint.h>

#include "int-telemetry.h"

struct int_header {
    uint8_t int_subtype;
    uint8_t int_control;
    uint8_t int_seqno;
    uint8_t int_bitmap;
};

#define INT_HEADER_SIZE 4

struct int_content {
    struct int_header int_current_header;
    LIST_STRUCT(int_telemetry_list);
};

#define INT_HDR_CONTROL_OVERFLOW_MASK 0x03

int int_engine_init(void);

int int_engine_output(void);

int add_telemetry_entry(void);

int int_engine_input(const uint8_t * buf, uint16_t len);

int remove_int_contents(void);

int remove_int_telemetry_entries(struct int_content * int_entry);

int embed_int_in_frame(void);

#endif