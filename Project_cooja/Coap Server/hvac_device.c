#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
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


#define PROGR_HOT 0
#define PROGR_COLD 1
#define PROGR_HUM_PLUS 2
#define PROGR_HUM_MINUS 3

/* Sensor configuration */
int actual_temp = 0;
int actual_hum = 0;

/* Client configuration */
int actual_status_temp_hum_sensors[MAX_SENSORS][2];
int n_temp_hum_sensor = 0;
char sensor_server_ip [MAX_SENSORS][UIPLIB_IPV6_MAX_STR_LEN];

/* Resources */
extern coap_resource_t  res_hvac;
extern bool actual_mode_hvac;
extern int program[4];

/* Registration handler Variables*/
int reg = 0;
static char room[MAX_ROOM_DIM];
static char my_id[4];

/* Sensor handler Variables*/
static int req = 0;
int response_ok = 0;

/* CLIENT HANDLERS */
void client_chunk_sensor_notify_handler(coap_message_t *response){
  const uint8_t *chunk;

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  response_ok = 1;

  coap_get_payload(response, &chunk);
}

void client_chunk_sensor_handler(coap_message_t *response){
  const uint8_t *chunk;
  char tmp[8];

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  response_ok = 1;

  coap_get_payload(response, &chunk);

  json_decode((char *)chunk,"temp",tmp, sizeof(int));
  actual_status_temp_hum_sensors[req][0] = atoi(tmp);

  json_decode((char *)chunk,"hum",tmp, sizeof(int));
  actual_status_temp_hum_sensors[req][1] = atoi(tmp);
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

void client_sensor_ip_chunk_handler(coap_message_t *response){
  const uint8_t *chunk;
  int dim = UIPLIB_IPV6_MAX_STR_LEN*MAX_SENSORS;
  char chunk_ip[dim];

  if(response == NULL) {
    puts("Request timed out");
    return;
  }
  reg = 1;

  coap_get_payload(response, &chunk);

  json_decode((char *)chunk,"ip",chunk_ip, sizeof(char)*dim);

  if(strcmp(chunk_ip, NOT_FOUND) == 0){
    n_temp_hum_sensor = 0;
  }else{
    char *token;
    n_temp_hum_sensor = 0;
    token = strtok(chunk_ip, ",");
    strcpy(sensor_server_ip [n_temp_hum_sensor], token);

    while( token != NULL && n_temp_hum_sensor < MAX_SENSORS) {
      n_temp_hum_sensor = n_temp_hum_sensor + 1;
      token = strtok(NULL, ",");

      if(token != NULL)
        strcpy(sensor_server_ip [n_temp_hum_sensor], token);
    }
    //LOG_INFO("Found %d temp hum sensors\n",n_temp_hum_sensor);
  }
}

/* PROCESS */
PROCESS(er_hvac_server, "Erbium HVAC Server");
AUTOSTART_PROCESSES(&er_hvac_server);

PROCESS_THREAD(er_hvac_server, ev, data){
  PROCESS_BEGIN();
  /* Client: Variables to manage the request for the server */
  static coap_endpoint_t sensor_server[MAX_SENSORS];
  static coap_message_t sensor_request[1];      /* This way the packet can be treated as pointer as usual. */

  static coap_endpoint_t server_cloud;
  static coap_message_t registration_request[1];      /* This way the packet can be treated as pointer as usual. */
  static coap_message_t sensor_ip_request[1];

  static char my_ip_addr[UIPLIB_IPV6_MAX_STR_LEN];
  static struct etimer e_timer;

  for (int i = 0 ; i < MAX_SENSORS ; i++){
    actual_status_temp_hum_sensors[i][0] = 0;
    actual_status_temp_hum_sensors[i][1] = 0;
  }

  LOG_INFO("Starting Erbium HVAC Server\n");

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
    sprintf(reg_payload, "{\"ip\":\"%s\", \"type\":\"%s\",\"uri\":\"%s\",\"operations\":\"%s\", \"post_format\":\"%s\",\"get_format\":\"%s\"}", my_ip_addr, HVAC_TYPE, HVAC_URL, HVAC_OPERATIONS, HVAC_PFORMAT, HVAC_GFORMAT);

    coap_set_payload(registration_request, (uint8_t *)reg_payload, strlen(reg_payload));

    COAP_BLOCKING_REQUEST(&server_cloud, registration_request, client_chunk_registration_handler);
  }

  /* BOOTSTRAP:  Attivo la risorsa ed inizio la fase di gestione della stessa */
  coap_activate_resource(&res_hvac, "hvac");
  etimer_set(&e_timer, CLOCK_SECOND * 2);

  /* BODY */
  while(1) {
    PROCESS_WAIT_EVENT();
    if(ev == PROCESS_EVENT_TIMER && data == &e_timer){
      if(actual_mode_hvac){
          /* Chiedo al SERVER CLOUD se ci sono sensori di temperatura presenti */
          coap_init_message(sensor_ip_request, COAP_TYPE_CON, COAP_GET, 0);
          coap_set_header_uri_path(sensor_ip_request, SENSOR_IP_REQUEST_URL);

          char reg_payload[100];
          sprintf(reg_payload, "{\"room\":\"%s\",\"type\":\"%s\",\"ip\":\"%s\"}", room, TEMP_HUM_TYPE, my_ip_addr);

          coap_set_payload(sensor_ip_request, (uint8_t *)reg_payload, strlen(reg_payload));
          COAP_BLOCKING_REQUEST(&server_cloud, sensor_ip_request, client_sensor_ip_chunk_handler);
          /* Faccio il parse dei server dei sensori trovati*/
          for(int i = 0 ; i < n_temp_hum_sensor ; i ++)
            coap_endpoint_parse(sensor_server_ip[i], strlen(sensor_server_ip[i]), &sensor_server[i]);

          /* Mando una richiesta ai sensori trovati */
          if(n_temp_hum_sensor != 0){
            for(req = 0 ; req < n_temp_hum_sensor ; req++){
              while(!response_ok){

                coap_init_message(sensor_request, COAP_TYPE_CON, COAP_GET, 0);
                coap_set_header_uri_path(sensor_request, TEMP_HUM_URL);

                COAP_BLOCKING_REQUEST(&sensor_server[req], sensor_request, client_chunk_sensor_handler);
              }
              response_ok = 0;
            }
          }
    	}
      res_hvac.trigger();
      //mando ai sensori trovati il programma attuale per la coerenza
      if(n_temp_hum_sensor != 0){
        for(req = 0 ; req < n_temp_hum_sensor ; req++){
          while(!response_ok){

            coap_init_message(sensor_request, COAP_TYPE_CON, COAP_POST, 0);
            coap_set_header_uri_path(sensor_request, TEMP_HUM_URL);

            char notify_payload[512];
            if((program[PROGR_HOT] == 0) && (program[PROGR_COLD] == 0)){
              to_json(notify_payload, "prog_temp", (void*)"none", STRING);
            }
            if(program[PROGR_HOT] == 1){
              to_json(notify_payload, "prog_temp", (void*)"+", STRING);
            }
            if(program[PROGR_COLD] == 1){
              to_json(notify_payload, "prog_temp", (void*)"-", STRING);
            }
            if((program[PROGR_HUM_PLUS] == 0) && (program[PROGR_HUM_MINUS] == 0)){
              to_json(notify_payload, "prog_hum", (void*)"none", STRING);
            }
            if(program[PROGR_HUM_PLUS] == 1){
              to_json(notify_payload, "prog_hum", (void*)"+", STRING);
            }
            if(program[PROGR_HUM_MINUS] == 1){
              to_json(notify_payload, "prog_hum", (void*)"-", STRING);
            }
            coap_set_payload(sensor_request, (uint8_t *)notify_payload, strlen(notify_payload));

            COAP_BLOCKING_REQUEST(&sensor_server[req], sensor_request, client_chunk_sensor_notify_handler);
          }
          response_ok = 0;
        }
      }
      etimer_set(&e_timer, CLOCK_SECOND*5 );
    }
  }
  PROCESS_END();
}
