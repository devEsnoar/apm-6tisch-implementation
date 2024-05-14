#include "contiki.h"
#include "fixed-app.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include "os/lib/random.h"

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

/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

#define SIZE  6
const int payload_sizes[SIZE]  = {10, 20, 30, 40, 45, 50};

/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 3
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
static char *service_urls[NUMBER_OF_URLS] =
{ ".well-known/core", "/send/dummy", "/test/hello" };

static int not_reached = 1;

static struct etimer et;
static struct etimer offset; 

PROCESS(er_fixed_app_traffic, "APM-6TiSCH - App Traffic Generator");

static uint8_t app_data_buffer[APP_DATA_BUFFER_SIZE];

static void 
fill_app_data_buffer(uint8_t len){
  for(int i = 0; i < len; i++){
    app_data_buffer[i] = '&';
  }
}

/* This function is will be passed to COAP_BLOCKING_REQUEST() to handle responses. */
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

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(er_fixed_app_traffic, ev, data)
{
  
  PROCESS_BEGIN();
  static coap_endpoint_t server_ep;
  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */
  static int len;
  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
  etimer_set(&offset, (node_id-1) * CLOCK_SECOND);
  PROCESS_YIELD_UNTIL(etimer_expired(&offset));
  random_init(0);

  etimer_set(&et, TOGGLE_INTERVAL * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&et)) {   
            if(rpl_is_reachable()) {
              if(not_reached) { 
                printf("### Joined the network ###\n");
                not_reached = 0;
              }
              printf("--- Sending data ---\n");

              /* prepare request, TID is set by COAP_BLOCKING_REQUEST() */
              coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
              coap_set_header_uri_path(request, service_urls[1]);     

              len = payload_sizes[random_rand() % SIZE];
              fill_app_data_buffer(len);
              printf("--- Sending > %.*s\n", len, app_data_buffer);
              coap_set_payload(request, app_data_buffer, len);

              LOG_INFO_COAP_EP(&server_ep);
              LOG_INFO_("\n");

              COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);

              printf("\n--Done--\n");

          }
        etimer_reset(&et);
    }
  }

  PROCESS_END();
}

void app_trafic_generator_init(void){
  process_start(&er_fixed_app_traffic, NULL);
}

