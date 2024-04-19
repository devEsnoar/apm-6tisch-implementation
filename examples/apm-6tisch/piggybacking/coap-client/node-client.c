#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "project-conf.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "../../fixed-app/fixed-app.h"

#include "contiki-net.h"


/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP


/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Client | APM-6TiSCH Piggybacking");
AUTOSTART_PROCESSES(&er_example_client);

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
  
  while(1) {
    PROCESS_YIELD();
  }

  PROCESS_END();
}