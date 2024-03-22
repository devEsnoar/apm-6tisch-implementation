
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
#if PLATFORM_HAS_LEDS
  extern coap_resource_t res_leds, res_toggle;
#endif

PROCESS(er_example_server, "Coap-6TiSCH Example Server");
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

  LOG_INFO("Starting Coap-6TiSCH Example Server\n");

  coap_activate_resource(&res_hello, "test/hello");
#if PLATFORM_HAS_LEDS
/*  coap_activate_resource(&res_leds, "actuators/leds"); */
  coap_activate_resource(&res_toggle, "actuators/toggle");
#endif

  
#if WITH_PERIODIC_ROUTES_PRINT
  static struct etimer et;
  /* Print out routing tables every minute */
  etimer_set(&et, CLOCK_SECOND * 60);
  while(1) {
    /* Used for non-regression testing */
    #if (UIP_MAX_ROUTES != 0)
      PRINTF("Routing entries: %u\n", uip_ds6_route_num_routes());
    #endif
    #if (UIP_SR_LINK_NUM != 0)
      PRINTF("Routing links: %u\n", uip_sr_num_nodes());
    #endif
    PROCESS_YIELD_UNTIL(etimer_expired(&et));
    etimer_reset(&et);
  }
#endif /* WITH_PERIODIC_ROUTES_PRINT */

  PROCESS_END();
}