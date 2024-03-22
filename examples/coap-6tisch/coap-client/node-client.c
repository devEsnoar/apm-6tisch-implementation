#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl-types.h"
#include "net/routing/rpl-lite/rpl-dag.h"
#include "os/lib/random.h"

#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"
#if PLATFORM_SUPPORTS_BUTTON_HAL
#include "dev/button-hal.h"
#else
#include "dev/button-sensor.h"
#endif

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

#if CONTIKI_TARGET_COOJA
#define SERVER_EP "coap://[fd00::201:1:1:1]"
#else
#define SERVER_EP "coap://[fd00::f6ce:36a2:9c50:4687]"
#endif

#define TOGGLE_INTERVAL 5

/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Coap-6TiSCH Example Client");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer et;

/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 3
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char *service_urls[NUMBER_OF_URLS] =
{ ".well-known/core", "/actuators/toggle", "/test/hello" };

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
void
client_chunk_handler(coap_message_t *response)
{
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }

  int len = coap_get_payload(response, &chunk);

  printf("|%.*s", len, (char *)chunk);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(er_example_client, ev, data)
{
  static coap_endpoint_t server_ep;
  PROCESS_BEGIN();
  
#if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_Z1
  if(node_id == 1) { /* Coordinator node. */
    NETSTACK_ROUTING.root_start();
  }
#endif
  NETSTACK_MAC.on();

  random_init(0);

  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */

  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
  rpl_dag_t * dag;
  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();

    if(etimer_expired(&et)) {
      dag = rpl_get_any_dag();
      if(dag->state == DAG_REACHABLE) {
        printf("--- Sending data ---\n");

        /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
        coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
        coap_set_header_uri_path(request, service_urls[1]);

        // const char msg_prefix[] = "Toggle!";
        // char msg[100];
        // int16_t size = sprintf(msg, "%s, Counter: %d", msg_prefix, random_rand()); 
        short unsigned int random = random_rand();
        // short unsigned int random = 25;
        printf("--- Sending > %d\n", random);
        coap_set_payload(request, (uint8_t *)&random, sizeof(short unsigned int));

        LOG_INFO_COAP_EP(&server_ep);
        LOG_INFO_("\n");

        COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

        printf("\n--Done--\n");

      }
      
      etimer_reset(&et);

#if PLATFORM_HAS_BUTTON
#if PLATFORM_SUPPORTS_BUTTON_HAL
    } else if(ev == button_hal_release_event) {
#else
    } else if(ev == sensors_event && data == &button_sensor) {
#endif

      /* send a request to notify the end of the process */

      coap_init_message(request, COAP_TYPE_CON, COAP_GET, 0);
      coap_set_header_uri_path(request, service_urls[2]);

      printf("--Requesting %s--\n", service_urls[2]);

      LOG_INFO_COAP_EP(&server_ep);
      LOG_INFO_("\n");

      COAP_BLOCKING_REQUEST(&server_ep, request,
                            client_chunk_handler);

      printf("\n--Done--\n");

      // uri_switch = (uri_switch + 1) % NUMBER_OF_URLS;
#endif /* PLATFORM_HAS_BUTTON */
    }
  }

  PROCESS_END();
}