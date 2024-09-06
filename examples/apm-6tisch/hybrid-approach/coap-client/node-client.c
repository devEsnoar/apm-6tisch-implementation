#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "contiki.h"
#include "project-conf.h"
#include "sys/node-id.h"
#include "sys/log.h"
#include "net/mac/tsch/tsch.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl.h"
#include "../../fixed-app/fixed-app.h"
#include "os/services/telemetry/telemetry-counter.h"
#include <math.h>


#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-blocking-api.h"


/* Log configuration */
#include "coap-log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL  LOG_LEVEL_APP

#if CONTIKI_TARGET_COOJA
#define SERVER_EP "coap://[fd00::201:1:1:1]"
#else
#define SERVER_EP "coap://[fd00::f6ce:36a2:9c50:4687]"
#endif

#define MONITORING_TOGGLE_INTERVAL 60

#define TELEMETRY_EXPERIMENT_SIZE TELEMETRY_SIZE
/*---------------------------------------------------------------------------*/
PROCESS(er_example_client, "Client | APM-6TiSCH Hybrid Approach");
AUTOSTART_PROCESSES(&er_example_client);

static struct etimer monitoring_et;
static struct etimer offset;

static int16_t diff_bytes_per_min;
static int16_t pkts_pending;
static uint16_t size_to_send;
uint16_t tm_packets_in_coap;

int first_time = 1;

int is_am_message = 0;

char buf[COAP_MAX_CHUNK_SIZE];

/* Example URIs that can be queried. */
#define NUMBER_OF_URLS 4
/* leading and ending slashes only for demo purposes, get cropped automatically when setting the Uri-Path */
static char *service_urls[NUMBER_OF_URLS] =
{ ".well-known/core", "/send/dummy", "/test/hello", "/send/telemetry"};

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
  tm_packets_in_coap = (uint16_t) floor( (COAP_MAX_CHUNK_SIZE - 14) / TELEMETRY_EXPERIMENT_SIZE);
  app_trafic_generator_init();
  reset_counter();
  static coap_message_t request[1];      /* This way the packet can be treated as pointer as usual. */
  LOG_DBG("tm_packets_in_coap %d = COAP_MAX_CHUNK_SIZE %d /  TELEMETRY_EXPERIMENT_SIZE %d: false\n", tm_packets_in_coap, COAP_MAX_CHUNK_SIZE, TELEMETRY_EXPERIMENT_SIZE);

  coap_endpoint_parse(SERVER_EP, strlen(SERVER_EP), &server_ep);
  
  etimer_set(&offset, ((node_id-1) + 3) * CLOCK_SECOND);
  PROCESS_YIELD_UNTIL(etimer_expired(&offset));

  etimer_set(&monitoring_et, MONITORING_TOGGLE_INTERVAL * CLOCK_SECOND);
  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&monitoring_et)) {
      if(rpl_is_reachable()){
        if(first_time){
          first_time = 0;
          reset_counter();
        } else {
          diff_bytes_per_min = MAX((int) USER_REQ_BYTES_PER_MIN - get_counter(), 0);
          LOG_DBG("Diff bytes per min %d = %d - %d\n", diff_bytes_per_min, USER_REQ_BYTES_PER_MIN, get_counter());
          pkts_pending = (uint16_t) floor(diff_bytes_per_min / TELEMETRY_EXPERIMENT_SIZE) + 1;
          LOG_DBG("Packets pending: %d\n", pkts_pending);
          if(diff_bytes_per_min > 0) {
            while(pkts_pending > 0){
              if (tm_packets_in_coap <= pkts_pending) {
                LOG_DBG("tm_packets_in_coap %d < pkts_pending %d: true\n", tm_packets_in_coap, pkts_pending);
                size_to_send = tm_packets_in_coap * TELEMETRY_EXPERIMENT_SIZE;
                pkts_pending = pkts_pending - tm_packets_in_coap;
              }
              else{
                LOG_DBG("tm_packets_in_coap %d < pkts_pending %d: false\n", tm_packets_in_coap, pkts_pending);
                size_to_send = pkts_pending * TELEMETRY_EXPERIMENT_SIZE;
                pkts_pending = 0;
              }
              LOG_DBG("size_to_send: %d\n", size_to_send);
              LOG_DBG("pkts_pending: %d\n", pkts_pending);
              printf("--- Sending monitoring data ---\n");

              coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
              coap_set_header_uri_path(request, service_urls[3]);
              printf("--- Sending > ");
              for(int i = 0; i < size_to_send; i++) {
                printf("%d", node_id);
                buf[i] = (uint8_t) node_id;
              }
              printf("\n");
              coap_set_payload(request, buf, size_to_send);

              LOG_INFO_COAP_EP(&server_ep);
              LOG_INFO_("\n");
              is_am_message = 1;
              COAP_BLOCKING_REQUEST(&server_ep, request, client_chunk_handler);
              printf("\n-- Done Sending Monitoring Data pkts_pending = %d --\n", pkts_pending);
            }
          }
          reset_counter();    
      }
      }
    etimer_reset(&monitoring_et);
    }
  }

  PROCESS_END();
}