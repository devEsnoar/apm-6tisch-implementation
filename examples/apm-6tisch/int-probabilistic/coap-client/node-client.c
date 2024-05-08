#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "sys/log.h"
#include "sys/node-id.h"
#include "net/mac/tsch/int/int-telemetry.h"

#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl.h"

#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"

#if CONTIKI_TARGET_COOJA
#define SERVER_EP "coap://[fd00::201:1:1:1]"
#else
#define SERVER_EP "coap://[fd00::f6ce:36a2:9c50:4687]"
#endif

#include "contiki-net.h"


/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 3
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
static char *service_urls[NUMBER_OF_URLS] =
{ ".well-known/core", "/send/dummy", "/test/hello" };

#define CONSUMING_INTERVAL 10

// const uint8_t PAYLOAD_SIZES[6] = {8, 12, 15, 18, 22, 25};
const uint8_t PAYLOAD_SIZES[6] = {10, 15, 20, 25, 30, 35};
static uint8_t app_data_buffer[50];
static uint8_t counter = 0;

/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Client | APM-6TiSCH INT Probabilistic");
AUTOSTART_PROCESSES(&er_example_client);

static int not_reached = 1;

static struct etimer et;

static void
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

static void 
fill_app_data_buffer(uint8_t len){
  for(int i = 0; i < len; i++){
    app_data_buffer[i] = '&';
  }
}
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
  
  static coap_endpoint_t server_ep;
  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */
  static int len;
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);

  etimer_set(&et, CONSUMING_INTERVAL * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();

    if(etimer_expired(&et)) {
      #if CONTIKI_TARGET_COOJA
      if(node_id == 3)
      #endif
      {
       if(rpl_is_reachable()) {
            if(not_reached) { 
              printf("### Joined the network ###\n");
              not_reached = 0;
            }
            printf("--- Sending data probabilistic ---\n");

            /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
            coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_uri_path(request, service_urls[1]);     

            len = PAYLOAD_SIZES[counter++ % 6];
            fill_app_data_buffer(len);
            printf("--- Sending > %.*s (%d)\n", len, app_data_buffer, len);
            coap_set_payload(request, app_data_buffer, len);

            LOG_INFO_COAP_EP(&server_ep);
            LOG_INFO_("\n");

            COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

            printf("\n--Done--\n");

        }
      }
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