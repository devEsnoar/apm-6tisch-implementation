#include "int.h"
#include "int-conf.h"
#include "contiki.h"
#include "stdio.h"
#include "net/packetbuf.h"
#include "net/netstack.h"
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

int inband_network_telemetry_output(void)
{

  char * dummy_telemetry = "ABC";

  uint8_t *p;
  struct ieee802154_ies ies; 

  if(!is_rpl_control_message && !packetbuf_holds_broadcast()
#if TSCH_WITH_6TOP
		&& !is_6top_control_message
#endif
  ){


    // TODO: Needs to be forwarded ?

        // TODO: INT Header Handler: Verify there is an INT Sub-IE (depends on int_input)


    // Frame Size Estimator
    int int_ies_size = strlen(dummy_telemetry) + 7;
    int total_packet_len = NETSTACK_FRAMER.length() + packetbuf_datalen();

    LOG_DBG("INT: In-Band Network Telemetry Output\n");

    // LOG_DBG("current payload buffer element: %01X %01X \n", *p, *(p + 1));

    if(int_ies_size + total_packet_len >= PACKETBUF_SIZE) {
      LOG_WARN("INT: Not enough space for INT data, max = %d, current_len = %d\n", PACKETBUF_SIZE, total_packet_len);
    }
    else {
			LOG_DBG("INT: Enough space for INT data, max = %d, current_len = %d\n", PACKETBUF_SIZE, total_packet_len);

      p = packetbuf_hdrptr();
      LOG_DBG("INT: Current values in buffer %0X %0X %0X %0X\n", p[0], p[1], p[2], p[3]);


      memset(&ies, 0, sizeof(ies));
      if(packetbuf_hdralloc(2) && frame80215e_create_ie_payload_list_termination(packetbuf_hdrptr(),
																												2,
																												&ies) < 0) {
				LOG_ERR("INT: int_output() fails because of Payload Termination 1 IE\n");
				return -1;
			}

      p = packetbuf_hdrptr();
      LOG_DBG("INT: Current values in buffer %0X %0X %0X %0X\n", p[0], p[1], p[2], p[3]);

      LOG_DBG("INT: Payload Termination 1 has been added\n");

      packetbuf_hdralloc(1 + strlen(dummy_telemetry));
      p = packetbuf_hdrptr();
      memcpy(p + 1, dummy_telemetry, strlen(dummy_telemetry));
      p[0] = INT_SUBIE_ID;

      p = packetbuf_hdrptr();
      LOG_DBG("INT: Current values in buffer %0X %0X %0X %0X %0X %0X\n", p[0], p[1], p[2], p[3], p[4], p[5]);
      

      memset(&ies, 0, sizeof(ies));
      ies.int_ie_content_len = 4;
      if(packetbuf_hdralloc(2) != 1 ||
        (frame80215e_create_ie_ietf_int(packetbuf_hdrptr(),
                                          2,
                                          &ies)) < 0) {
        LOG_ERR("INT: int_output() fails because of Payload IE Header\n");
        return -1;
      }

      p = packetbuf_hdrptr();
      LOG_DBG("INT: Current values in buffer %0X %0X %0X %0X %0X %0X\n", p[0], p[1], p[2], p[3], p[4], p[5]);


      memset(&ies, 0, sizeof(ies));
			if(packetbuf_hdralloc(2) &&
				frame80215e_create_ie_header_list_termination_1(packetbuf_hdrptr(),
																												2,
																												&ies) < 0) {
				LOG_ERR("INT: int_output() fails because of Header Termination 1 IE\n");
				return -1;
			}
      LOG_DBG("INT: Payload Header 1 has been added\n");
      
      p = packetbuf_hdrptr();
      LOG_DBG("INT: Current values in buffer %0X %0X %0X %0X %0X %0X\n", p[0], p[1], p[2], p[3], p[4], p[5]);

			packetbuf_set_attr(PACKETBUF_ATTR_MAC_METADATA, 1);

			

    }

    
  }

  is_rpl_control_message = 0;
#if TSCH_WITH_6TOP
  is_6top_control_message = 0;
#endif

	return 0;
}

void inband_network_telemetry_input(void)
{ 
  LOG_DBG("INT: In-Band Network Telemetry Input\n");

  uint8_t *hdr_ptr, *payload_ptr;
  uint16_t hdr_len, payload_len;

  frame802154_t frame;
  struct ieee802154_ies ies;

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

  if(frame.fcf.ie_list_present && frame802154e_parse_information_elements(payload_ptr, payload_len, &ies) >= 0)
    {
      LOG_DBG("INT: Information Elements Parsed\n");
      packetbuf_hdrreduce(2);
      LOG_DBG("INT: Header Pointer reduced by 2 (Header List Termination)\n");
      
      packetbuf_hdrreduce(2);
      LOG_DBG("INT: Header Pointer reduced by 2 (IETF Descriptor)\n");

      packetbuf_hdrreduce(4);
      LOG_DBG("INT: Header Pointer reduced by 4 (INT Content)\n");

      uint8_t *ptr = packetbuf_dataptr();
      
      if(ptr[0] == 0x00 && ptr[1] == 0xf8) {
        packetbuf_hdrreduce(2);
        LOG_DBG("INT: Payload Pointer reduced by 2 (Payload List Termination)\n");
      }

      const uint8_t * received_telemetry = ies.int_ie_content_ptr;
      LOG_DBG("INT: Received telemetry:");
      for(int i = 0; i < ies.int_ie_content_len; i++){
        LOG_DBG_("%c", received_telemetry[i]);
      }
      LOG_DBG_("\n");
      
    }
    else
    {
      LOG_DBG("INT: No IE in Frame\n");
    }
    
    
    // packetbuf_hdrreduce(2);
    // LOG_DBG("INT: Header Pointer reduced by 2 (Header List Termination)\n");

}

