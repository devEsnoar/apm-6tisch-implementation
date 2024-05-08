
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coap-engine.h"

#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/int/int-telemetry.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "project-conf.h"

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

/*
 * Resources to be activated need to be imported through the extern keyword.
 * The build system automatically compiles the resources in the corresponding sub-directory.
 */
extern coap_resource_t
  res_hello;

extern coap_resource_t 
  res_send_dummy;


PROCESS(er_example_server, "Server | APM-6TiSCH INT Probabilistic");
AUTOSTART_PROCESSES(&er_example_server);

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(er_example_server, ev, data)
{
  PROCESS_BEGIN();
#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_Z1
  if(node_id == 1) { /* Coordinator node. */
    NETSTACK_ROUTING.root_start();
  }
#else
  if(node_id == 18055) { /* Coordinator node. */
    NETSTACK_ROUTING.root_start();
  }
#endif
  NETSTACK_MAC.on();

  PROCESS_PAUSE();

  LOG_INFO("Starting Coap-6TiSCH Server - INT\n");

  coap_activate_resource(&res_hello, "test/hello");
  coap_activate_resource(&res_send_dummy, "send/dummy");

  static struct etimer telemetry_et;
  etimer_set(&telemetry_et, CLOCK_SECOND * 30);
  PRINTF("Timer activated\n");
  while(1) {
    PROCESS_YIELD();
    
    if(etimer_expired(&telemetry_et)){
      PRINTF("Timer expired\n");
      struct telemetry_model tm_entry;
      memset(&tm_entry, 0, sizeof(struct telemetry_model));
      {
        while(!app_get_last_telemetry_entry(&tm_entry)){

          #if INT_CONF_TELEMETRY_EXPERIMENT
          // char buf[20];
          // snprintf(buf, sizeof(buf), "%d", tm_entry.dummy_data[0]);
          // for (int i = 1; i < INT_CONF_TELEMETRY_EXPERIMENT_SIZE; ++i) {
          //   snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "%d", tm_entry.dummy_data[i]);
          // }
          // snprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), "\n");
          // PRINTF("EXPERIMENT: Consumed %d Bytes of telemetry: %s", INT_CONF_TELEMETRY_EXPERIMENT_SIZE, buf);
          #else
          uint16_t channel = (tm_entry.channel_and_timestamp & 0xF000) >> 12;
          uint16_t timestamp = (tm_entry.channel_and_timestamp & 0x0FFF);
          PRINTF("Consuming telemetry: Node ID: %d, Channel and timestamp: %d, %d, RSSI: %d\n", tm_entry.node_id, channel, timestamp, (int8_t) tm_entry.rssi);
          #endif
        
          
        }
        PRINTF("Consuming telemetry: Nothing else in list\n");
      }
      etimer_reset(&telemetry_et);
    }
      
    
  }

  PROCESS_END();
}