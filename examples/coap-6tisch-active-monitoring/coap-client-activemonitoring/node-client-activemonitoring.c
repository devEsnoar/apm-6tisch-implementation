#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "project-conf.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/ipv6/uip-ds6-route.h"
#include "net/ipv6/uip-sr.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl.h"
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

#define TOGGLE_INTERVAL 10
#define MONITORING_TOGGLE_INTERVAL 7

#define INT_CONF_TELEMETRY_EXPERIMENT_SIZE 2
/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Coap-6TiSCH Example Client");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer et;
static struct etimer monitoring_et;
static struct etimer offset;

/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 3
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
char *service_urls[NUMBER_OF_URLS] =
{ ".well-known/core", "/send/dummy", "/test/hello" };

static int not_reached = 1;
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

  etimer_set(&offset, (node_id-1) * CLOCK_SECOND);
  PROCESS_YIELD_UNTIL(etimer_expired(&offset));


  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);
  etimer_set(&monitoring_et, MONITORING_TOGGLE_INTERVAL * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();

    if(etimer_expired(&et)) {
        {
      
            if(rpl_is_reachable()) {
              if(not_reached) { 
                printf("### Joined the network ###\n");
                not_reached = 0;
              }
              printf("--- Sending data ---\n");

              coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
              coap_set_header_uri_path(request, service_urls[1]);

              char dummy[3] = {'D', 'A', '\0'};
              printf("--- Sending > %s\n", dummy);
              coap_set_payload(request, dummy, sizeof(dummy));

              LOG_INFO_COAP_EP(&server_ep);
              LOG_INFO_("\n");

              COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

              printf("\n--Done--\n");
              /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
          }
        }      
      etimer_reset(&et);
    }

    if(etimer_expired(&monitoring_et)) {
      if(rpl_is_reachable()){
        printf("--- Sending monitoring data ---\n");

          coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
          coap_set_header_uri_path(request, service_urls[1]);
          
          char buf[INT_CONF_TELEMETRY_EXPERIMENT_SIZE];
          printf("--- Sending > ");
          for(int i = 0; i < INT_CONF_TELEMETRY_EXPERIMENT_SIZE; i++) {
            printf("%d", node_id);
            buf[i] = (uint8_t) node_id;
          }
          printf("\n");

          coap_set_payload(request, buf, INT_CONF_TELEMETRY_EXPERIMENT_SIZE);

          LOG_INFO_COAP_EP(&server_ep);
          LOG_INFO_("\n");

          COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

          printf("\n-- Done Sending Monitoring Data  --\n");


      }
      etimer_reset(&monitoring_et);
    }
  }

  PROCESS_END();
}