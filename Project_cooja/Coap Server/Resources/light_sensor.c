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

#define MIN_LIGHT 0
#define MAX_LIGHT 100

static int light = 30;
static int level = 0;

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

EVENT_RESOURCE(res_light_s,
  "title=\"light sensor\";"
  "methods=\"GET\";"
  "payload={\"light\":\"0..100\"};"
  "rt=\"sensor\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);

/*Change values function*/
static void
change_light(void){
  int change = (random_rand() % 3) -1;
  light += change;
  //Random change of maximum 1% of the light percentage,
  if(level < 35){
    if(light > MAX_LIGHT/3)
      light = MAX_LIGHT/3;
    if(light < MIN_LIGHT)
      light = MIN_LIGHT;
  }
  if((level >= 35)&&(level < 70)){
    if(light > ((MAX_LIGHT/3)*2))
      light = ((MAX_LIGHT/3)*2);
    if(light < MAX_LIGHT/3)
      light = MAX_LIGHT/3;
  }
  if(level >= 70){
    if(light > MAX_LIGHT)
      light = MAX_LIGHT;
    if(light < ((MAX_LIGHT/3)*2))
      light = ((MAX_LIGHT/3)*2);
  }
}

/*Called by a timed periodic event*/
static void
res_event_handler(void){
  change_light();
  //LOG_DBG("L=%d\n",light);
  // Notify all the observers
  coap_notify_observers(&res_light_s);
}

/*
  * POST/PUT JSON payload:
  *   {"level":"0..100"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  const uint8_t *payload = NULL;

  char roller_lvl[5];
  int success = 1;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"level",roller_lvl,sizeof(char)*5);
    if(strcmp(roller_lvl, "") != 0){
      //LOG_DBG("roller_lvl %s\n", roller_lvl);

      level = atoi(roller_lvl);
    }else{ // no key giusta
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
  *   ""
  * Format of JSON response:
  * {"response_code":"1","light":"0..100"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);
  snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"light\":\"%d\"}",RESPONSE_OK, light);

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
