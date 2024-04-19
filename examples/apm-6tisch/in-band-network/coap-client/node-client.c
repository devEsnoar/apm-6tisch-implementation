#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "net/mac/tsch/int/int-telemetry.h"
#include "../../fixed-app/fixed-app.h"

#include "contiki-net.h"


/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

#define CONSUMING_INTERVAL 10

/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Client | APM-6TiSCH INT");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer et;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(er_example_client, ev, data)
{
  
  PROCESS_BEGIN();
  
#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_Z1
  if(node_id == 1) { /* Coordinator node. */
    NETSTACK_ROUTING.root_start();
  }
#endif
  NETSTACK_MAC.on();

  app_trafic_generator_init();

  etimer_set(&et, CONSUMING_INTERVAL * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();

    if(etimer_expired(&et)) {
      struct telemetry_model tm_entry;
      memset(&tm_entry, 0, sizeof(struct telemetry_model));
      {
        while(!app_get_last_telemetry_entry(&tm_entry)){
          
        }
      }
      etimer_reset(&et);
    }
  }

  PROCESS_END();
}