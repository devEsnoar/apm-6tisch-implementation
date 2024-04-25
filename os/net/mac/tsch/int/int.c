#include "int.h"
#include "int-engine.h"
#include "int-conf.h"
#include "contiki.h"
#include "stdio.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
#include "net/routing/routing.h"
#include "net/mac/framer/framer-802154.h"
#include "net/mac/framer/frame802154e-ie.h"
#include "lib/assert.h"
/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "INT"
#define LOG_LEVEL LOG_LEVEL_INT

extern int is_rpl_control_message;

#if TSCH_WITH_6TOP
extern int is_6top_control_message;
#endif

extern int INT_SIZE_OVERHEAD;
extern int is_source_appdata;

static int received_int = 0;
int inband_network_telemetry_output(void)
{

  LOG_INFO("INT Output: In-Band Network Telemetry Output\n");
  if(!is_rpl_control_message && !packetbuf_holds_broadcast() && ((received_int || is_source_appdata) && !NETSTACK_ROUTING.node_is_root())
#if TSCH_WITH_6TOP
		&& !is_6top_control_message
#endif
  ){ 
    int ret;
    LOG_DBG("INT Output: Feasible INT\n");
    if(!int_engine_output() && !embed_int_in_frame()){
      ret = 0;
    }
    else{
      LOG_ERR("INT: int_engine_output() and/or embed_int_in_frame() failed\n");
      ret = 1;
    }
    is_source_appdata = 0;
    return ret;

  }
  else {
    LOG_INFO("INT Output: Discarded => TSCH, 6Top control message, broadcast message or RPL message\n");
  }

  is_rpl_control_message = 0;
#if TSCH_WITH_6TOP
  is_6top_control_message = 0;
#endif

	return 0;
}

void inband_network_telemetry_input(void)
{ 
  LOG_INFO("INT Input: In-Band Network Telemetry Input\n");

  
    uint8_t *hdr_ptr, *payload_ptr;
    uint16_t hdr_len, payload_len;

    frame802154_t frame;
    struct ieee802154_ies ies;
    received_int = 0;
    payload_ptr = packetbuf_dataptr();
    payload_len = packetbuf_datalen();
    hdr_len = packetbuf_hdrlen();
    hdr_ptr = payload_ptr - hdr_len;

    if(frame802154_parse(hdr_ptr, hdr_len, &frame) == 0) {
        /* parse error; should not occur, anyway */
        LOG_ERR("INT: frame802154_parse error\n");
        return;
    }

    assert(frame.fcf.frame_version == FRAME802154_IEEE802154_2015);
    assert(frame.fcf.frame_type == FRAME802154_DATAFRAME);
    memset(&ies, 0, sizeof(ies));

    if(frame.fcf.ie_list_present && frame802154e_parse_information_elements(payload_ptr, payload_len, &ies) >= 0 && ies.int_ie_content_len > 0) {
        int_engine_input(ies.int_ie_content_ptr, ies.int_ie_content_len);
        packetbuf_hdrreduce(INT_SIZE_OVERHEAD + ies.int_ie_content_len);
        received_int = 1;
    }
    else {
        LOG_INFO("INT Engine: No IEs in Frame\n");
    }
}

void inband_network_telemetry_init(void) {
  int_engine_init();
  telemetry_init();

}

