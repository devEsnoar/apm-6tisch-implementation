
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coap-engine.h"

#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"


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


PROCESS(er_example_server, "Server | APM-6TiSCH Active Monitoring");
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

  LOG_INFO("Starting Coap-6TiSCH Server - Active Monitoring\n");

  coap_activate_resource(&res_hello, "test/hello");
  coap_activate_resource(&res_send_dummy, "send/dummy");

  static struct etimer telemetry_et;
  etimer_set(&telemetry_et, CLOCK_SECOND * 30);
  PRINTF("Timer activated\n");
  while(1) {
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}