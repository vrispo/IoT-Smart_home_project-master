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
#include "net/ipv6/uiplib.h"
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

/* Client configuration */
int actual_status_light_sensors[MAX_SENSORS];
int n_light_sensor;
int actual_status_presence_sensors[MAX_SENSORS];
int n_presence_sensor;
char presence_sensor_server_ip [MAX_SENSORS][UIPLIB_IPV6_MAX_STR_LEN];
char light_sensor_server_ip [MAX_SENSORS][UIPLIB_IPV6_MAX_STR_LEN];

/* Resources */
extern coap_resource_t  res_light_bulb;
#define M_AUTO 2
extern  int actual_light_bulb_mode;

/* Registration handler Variables*/
int reg = 0;
static char room[MAX_ROOM_DIM];
static char my_id[4];

/* Sensor handler Variables*/
static int req = 0;
int response_ok = 0;

/* CLIENT HANDLERS */
void client_chunk_presence_sensor_handler(coap_message_t *response){
  const uint8_t *chunk;
  char tmp[8];
  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  response_ok = 1;

  coap_get_payload(response, &chunk);
  json_decode((char *)chunk,"presence",tmp, sizeof(char)*8);
  actual_status_presence_sensors[req] = atoi(tmp);

  //LOG_DBG("Presence sensor n. %d value %d\n", req, actual_status_presence_sensors[req]);
}

void client_chunk_light_sensor_handler(coap_message_t *response){
  const uint8_t *chunk;
  char tmp[8];

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  response_ok = 1;

  coap_get_payload(response, &chunk);
  json_decode((char *)chunk,"light",tmp, sizeof(char)*8);
  actual_status_light_sensors[req] = atoi(tmp);

  //LOG_DBG("Light sensor n. %d value %d\n", req, actual_status_light_sensors[req]);
}


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

void client_presence_sensor_ip_chunk_handler(coap_message_t *response){
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  reg = 1;

  coap_get_payload(response, &chunk);

  int dim = UIPLIB_IPV6_MAX_STR_LEN*MAX_SENSORS;
  char chunk_ip[dim];

  json_decode((char *)chunk,"ip",chunk_ip, sizeof(char)*dim);

  if(strcmp(chunk_ip, NOT_FOUND) == 0){
    n_presence_sensor = 0;
  }else{
    char *token;
    n_presence_sensor = 0;
    token = strtok(chunk_ip, ",");
    strcpy(presence_sensor_server_ip [n_presence_sensor], token);

    while( token != NULL && n_presence_sensor < MAX_SENSORS) {
      n_presence_sensor = n_presence_sensor + 1;
      token = strtok(NULL, ",");

      if(token != NULL)
        strcpy(presence_sensor_server_ip [n_presence_sensor], token);
    }
    //LOG_INFO("Found %d presence sensors\n",n_presence_sensor);
  }
}

void client_light_sensor_ip_chunk_handler(coap_message_t *response){
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  reg = 1;

  coap_get_payload(response, &chunk);

  int dim = UIPLIB_IPV6_MAX_STR_LEN*MAX_SENSORS;
  char chunk_ip[dim];

  json_decode((char *)chunk,"ip",chunk_ip, sizeof(char)*dim);

  if(strcmp(chunk_ip, NOT_FOUND) == 0){
    n_light_sensor = 0;
  }else{
    char *token;
    n_light_sensor = 0;
    token = strtok(chunk_ip, ",");
    strcpy(light_sensor_server_ip [n_light_sensor], token);

    while( token != NULL && n_light_sensor < MAX_SENSORS) {
      n_light_sensor = n_light_sensor + 1;
      token = strtok(NULL, ",");

      if(token != NULL)
        strcpy(light_sensor_server_ip [n_light_sensor], token);
    }
    //LOG_INFO("Found %d light sensors\n",n_light_sensor);
  }
}

/* PROCESS */
PROCESS(er_light_bulb_server, "Erbium Light Bulb Server");
AUTOSTART_PROCESSES(&er_light_bulb_server);

PROCESS_THREAD(er_light_bulb_server, ev, data){
  PROCESS_BEGIN();
  /* Client: Variables to manage the request for the server */
  static coap_endpoint_t light_sensor_server[MAX_SENSORS];
  static coap_endpoint_t presence_sensor_server[MAX_SENSORS];
  static coap_message_t light_sensor_request[1];      /* This way the packet can be treated as pointer as usual. */
  static coap_message_t presence_sensor_request[1];      /* This way the packet can be treated as pointer as usual. */

  static coap_endpoint_t server_cloud;
  static coap_message_t registration_request[1];      /* This way the packet can be treated as pointer as usual. */
  static coap_message_t light_sensor_ip_request[1];
  static coap_message_t presence_sensor_ip_request[1];

  static char my_ip_addr[UIPLIB_IPV6_MAX_STR_LEN];
  static struct etimer e_timer;

  for (int i = 0 ; i < MAX_SENSORS ; i++){
    actual_status_light_sensors[i] = 0;
    actual_status_presence_sensors[i] = 0;
  }

  LOG_INFO("Starting Erbium Light Bulb Server\n");

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
    sprintf(reg_payload, "{\"ip\":\"%s\", \"type\":\"%s\",\"uri\":\"%s\",\"operations\":\"%s\", \"post_format\":\"%s\",\"get_format\":\"%s\"}", my_ip_addr, LIGHT_BULB_TYPE, LIGHT_BULB_URL, LIGHT_BULB_OPERATIONS, LIGHT_BULB_PFORMAT, NONE_FORMAT);

    coap_set_payload(registration_request, (uint8_t *)reg_payload, strlen(reg_payload));

    COAP_BLOCKING_REQUEST(&server_cloud, registration_request, client_chunk_registration_handler);
  }

  /* BOOTSTRAP:  Attivo la risorsa ed inizio la fase di gestione della stessa */
  coap_activate_resource(&res_light_bulb, "light_bulb");
  etimer_set(&e_timer, CLOCK_SECOND * 2);

  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER && data == &e_timer){
      if(actual_light_bulb_mode == M_AUTO){
          /* Chiedo al SERVER CLOUD se ci sono sensori di presenza presenti */
          coap_init_message(presence_sensor_ip_request, COAP_TYPE_CON, COAP_GET, 0);
          coap_set_header_uri_path(presence_sensor_ip_request, SENSOR_IP_REQUEST_URL);

          char reg_payload[100];
          sprintf(reg_payload, "{\"room\":\"%s\",\"type\":\"%s\",\"ip\":\"%s\"}", room, PRES_SENS_TYPE, my_ip_addr);

          coap_set_payload(presence_sensor_ip_request, (uint8_t *)reg_payload, strlen(reg_payload));
          COAP_BLOCKING_REQUEST(&server_cloud, presence_sensor_ip_request, client_presence_sensor_ip_chunk_handler);
          /* Faccio il parse dei server dei sensori trovati*/
          for(int i = 0 ; i < n_presence_sensor ; i ++)
            coap_endpoint_parse(presence_sensor_server_ip[i], strlen(presence_sensor_server_ip[i]), &presence_sensor_server[i]);
          /* Mando una richiesta ai sensori trovati */
          if(n_presence_sensor != 0){
            for(req = 0 ; req < n_presence_sensor ; req++){
              while(!response_ok){
                coap_init_message(presence_sensor_request, COAP_TYPE_CON, COAP_GET, 0);
                coap_set_header_uri_path(presence_sensor_request, PRES_SENS_URL);

                COAP_BLOCKING_REQUEST(&presence_sensor_server[req], presence_sensor_request, client_chunk_presence_sensor_handler);
              }
              response_ok = 0;
            }
          }
          /* Chiedo al SERVER CLOUD se ci sono sensori di luce presenti */
          coap_init_message(light_sensor_ip_request, COAP_TYPE_CON, COAP_GET, 0);
          coap_set_header_uri_path(light_sensor_ip_request, SENSOR_IP_REQUEST_URL);

          sprintf(reg_payload, "{\"room\":\"%s\",\"type\":\"%s\",\"ip\":\"%s\"}", room, LIGHT_SENS_TYPE, my_ip_addr);

          coap_set_payload(light_sensor_ip_request, (uint8_t *)reg_payload, strlen(reg_payload));
          COAP_BLOCKING_REQUEST(&server_cloud, light_sensor_ip_request, client_light_sensor_ip_chunk_handler);
          /* Faccio il parse dei server dei sensori trovati*/
          for(int i = 0 ; i < n_light_sensor ; i ++)
            coap_endpoint_parse(light_sensor_server_ip[i], strlen(light_sensor_server_ip[i]), &light_sensor_server[i]);

          /* Mando una richiesta ai sensori trovati */
          if(n_light_sensor != 0){
            for(req = 0 ; req < n_light_sensor ; req++){
              while(!response_ok){
                coap_init_message(light_sensor_request, COAP_TYPE_CON, COAP_GET, 0);
                coap_set_header_uri_path(light_sensor_request, LIGHT_SENS_URL);

                COAP_BLOCKING_REQUEST(&light_sensor_server[req], light_sensor_request, client_chunk_light_sensor_handler);
              }
              response_ok = 0;
            }
          }
      }
      res_light_bulb.trigger();
      etimer_set(&e_timer, CLOCK_SECOND );
    }
  }
  PROCESS_END();
}
