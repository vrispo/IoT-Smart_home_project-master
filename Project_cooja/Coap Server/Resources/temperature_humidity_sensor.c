#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coap-engine.h"
#include "lib/random.h"

#include "json.h"
#include "utilities.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#define MIN_TEMP -10
#define MAX_TEMP 40
#define MIN_HUM 0
#define MAX_HUM 100


static int temperature = 20;
static int humidity = 30;
static int prog_hum = 0;
static int prog_temp = 0;


static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

EVENT_RESOURCE(res_temp_hum,
  "title=\"temperature_humidity sensor\";"
  "methods=\"GET\";"
  "payload={\"temp\":\"-10..40\",\"hum\":\"0..100\"};"
  "rt=\"sensor\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);

/* Change values function */
static void
change_value(void){
  if(prog_temp == 0){
    //Random change of maximum 1 degree of the temperature
    int change = (random_rand() % 3) -1;
    temperature += change;
    if(temperature > MAX_TEMP)
      temperature = MAX_TEMP;
    if(temperature < MIN_TEMP)
      temperature = MIN_TEMP;
  }
  if(prog_temp == -1){
    int change = (random_rand() % 2);
    temperature -= change;
    if(temperature < MIN_TEMP)
      temperature = MIN_TEMP;
  }
  if(prog_temp == 1){
    int change = (random_rand() % 2);
    temperature += change;
    if(temperature > MAX_TEMP)
      temperature = MAX_TEMP;
  }
  if(prog_hum == 0){
    //Random change of maximum 1% of the humidity percentage
    int change = (random_rand() % 3) -1;
    humidity += change;
    if(humidity > MAX_HUM)
      humidity = MAX_HUM;
    if(humidity < MIN_HUM)
      humidity = MIN_HUM;
  }
  if(prog_hum == -1){
    int change = (random_rand() % 2);
    humidity -= change;
    if(humidity < MIN_HUM)
      humidity = MIN_HUM;
  }
  if(prog_hum == 1){
    int change = (random_rand() % 2);
    humidity += change;
    if(humidity > MAX_HUM)
      humidity = MAX_HUM;
  }
}

/*Called by a timed periodic event*/
static void
res_event_handler(void){
  change_value();
  //LOG_DBG("T=%d H=%d\n",temperature,humidity);
  // Notify all the observers
  coap_notify_observers(&res_temp_hum);
}

/*
  * POST/PUT JSON payload:
  *   {"prog_temp":"-|+|none","prog_hum":"-|+|none"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  const uint8_t *payload = NULL;

  char prog_temp_s[10];
  char prog_hum_s[10];
  int success = 1;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"prog_temp",prog_temp_s,sizeof(char)*10);
    if(strcmp(prog_temp_s, "") != 0){
      //LOG_DBG("prog_temp %s\n", prog_temp_s);

      if(strcmp(prog_temp_s, "+") == 0) {
        prog_temp = 1;
      }
      else{
        if(strcmp(prog_temp_s, "-") == 0) {
          prog_temp = -1;
        }
        else{
          if(strcmp(prog_temp_s, "none") == 0){
            prog_temp = 0;
          }else{//value di prog_temp errato
            success = 0;
          }
        }
      }
    }
    json_decode(payload_s,"prog_hum",prog_hum_s,sizeof(char)*10);
    if(strcmp(prog_hum_s, "") != 0){
      //LOG_DBG("prog_hum %s\n", prog_hum_s);

      if(strcmp(prog_hum_s, "+") == 0) {
        prog_hum = 1;
      }
      else{
        if(strcmp(prog_hum_s, "-") == 0) {
          prog_hum = -1;
        }
        else{
          if(strcmp(prog_hum_s, "none") == 0){
            prog_hum = 0;
          }else{//value di prog_hum errato
            success = 0;
          }
        }
      }
    }else if((strcmp(prog_hum_s, "") == 0 ) && (strcmp(prog_temp_s, "") == 0 )){
      success = 0;
    }
  }else{ //nessun payload presente
    success = 0;
  }

  if(!success) {
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\"}", RESPONSE_BAD);
  }else{
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\"}", RESPONSE_OK);
  }
  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}

/*
  * GET JSON payload:
  *   "" to get all the value
  *   {"value":"temp|hum"}
  * Format of JSON response:
  *   {"response_code":"1","temp":"int","hum":"int"}
  *   {"response_code":"1","temp":"int"}
  *   {"response_code":"1","hum":"int"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  const uint8_t *payload = NULL;
  char payload_s[200];
  char value[10];

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    sprintf(payload_s,"%s",(char*)payload);
    json_decode(payload_s,"value",value,sizeof(char)*10);

    if(strcmp(value, "") != 0){
      coap_set_header_content_format(response, APPLICATION_JSON);
      if(strncmp(value, "temp",1) == 0)
        snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"temp\":\"%d\"}", RESPONSE_OK, temperature);
      else
        if(strncmp(value, "hum",1) == 0)
          snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE,"{\"response_code\":\"%d\",\"hum\":\"%d\"}", RESPONSE_OK, humidity);
        else
          snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\"}", RESPONSE_BAD);
    }else{
      coap_set_header_content_format(response, APPLICATION_JSON);
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"temp\":\"%d\",\"hum\":\"%d\"}", RESPONSE_OK, temperature, humidity);
    }
  }else{
    coap_set_header_content_format(response, APPLICATION_JSON);
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"temp\":\"%d\",\"hum\":\"%d\"}", RESPONSE_OK, temperature, humidity);
  }

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
