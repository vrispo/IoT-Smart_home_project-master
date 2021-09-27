#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "contiki.h"
#include "coap-engine.h"
#include "sys/etimer.h"

/* Client configuration*/
#include "contiki-net.h"
#include "coap-blocking-api.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"

/* Net configuration */
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/uip-debug.h"

/* Our libraries */
#include "networking_lib.h"
#include "json.h"
#include "utilities.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

/* Resources */
extern coap_resource_t  res_presence_s;

/* Registration handler Variables*/
int reg = 0;
static char room[MAX_ROOM_DIM];
static char my_id[4];

/* CLIENT HANDLERS */
void client_chunk_registration_handler(coap_message_t *response){
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  reg = 1;

  coap_get_payload(response, &chunk);

  json_decode((char *)chunk,"room",room, sizeof(char)*MAX_ROOM_DIM);
  LOG_INFO("my room:%s\n",room);

  json_decode((char *)chunk,"id",my_id, sizeof(char)*4);
  LOG_INFO("my id:%s\n",my_id);
}

PROCESS(er_presence_s_server, "Erbium Presence Server");
AUTOSTART_PROCESSES(&er_presence_s_server);

PROCESS_THREAD(er_presence_s_server, ev, data)
{
  PROCESS_BEGIN();
  /* Client: Variables to manage the request for the server */
  static coap_endpoint_t server_cloud;
  static coap_message_t registration_request[1];

  static struct etimer e_timer;
  static char my_ip_addr[UIPLIB_IPV6_MAX_STR_LEN];

  LOG_INFO("Starting Erbium Presence Server\n");

  /* BOOTSTRAP: Fase di inizializzazione dell'interfaccia di rete */
  set_global_address();
  //print_addresses();
  get_my_remote_address(my_ip_addr);
  LOG_INFO("my IP addr: %s \n",my_ip_addr);

  etimer_set(&e_timer, CLOCK_SECOND * 2);
  LOG_INFO("Not reachable yet\n");
  while(!NETSTACK_ROUTING.node_is_reachable()){
    etimer_set(&e_timer, CLOCK_SECOND * 2);
    PROCESS_WAIT_EVENT();
  }
  LOG_INFO("Reachable\n");

  /* BOOTSTRAP: Fase di registrazione */
  coap_endpoint_parse(SERVER_CLOUD, strlen(SERVER_CLOUD), &server_cloud);

  while(!reg){
    LOG_INFO("Richiedo registrazione\n");

    coap_init_message(registration_request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(registration_request, REGISTRATION_SERVICE_URL);

    char reg_payload[512];
    sprintf(reg_payload, "{\"ip\":\"%s\", \"type\":\"%s\",\"uri\":\"%s\",\"operations\":\"%s\", \"post_format\":\"%s\",\"get_format\":\"%s\"}", my_ip_addr, PRES_SENS_TYPE, PRES_SENS_URL, PRES_SENS_OPERATIONS, NONE_FORMAT, NONE_FORMAT);

    coap_set_payload(registration_request, (uint8_t *)reg_payload, strlen(reg_payload));

    COAP_BLOCKING_REQUEST(&server_cloud, registration_request, client_chunk_registration_handler);
  }

  /* BOOTSTRAP:  Attivo la risorsa ed inizio la fase di gestione della stessa */
  coap_activate_resource(&res_presence_s, "presence_s");
  etimer_set(&e_timer, CLOCK_SECOND * 2);

  while(1) {
    PROCESS_WAIT_EVENT();

    if(ev == PROCESS_EVENT_TIMER && data == &e_timer){
      res_presence_s.trigger();
      etimer_set(&e_timer, CLOCK_SECOND * 4);
    }
  }
  PROCESS_END();
}
