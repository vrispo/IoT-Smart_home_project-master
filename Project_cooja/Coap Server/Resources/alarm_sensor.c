#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "coap-engine.h"
#include "lib/random.h"

#include "utilities.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP


#define PROBABILITY_SOUND 10

static bool alarm = false;

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

EVENT_RESOURCE(res_alarm_s,
  "title=\"alarm sensor\";"
  "methods=\"GET\";"
  "payload={\"alarm\":\"0|1\"};"
  "rt=\"sensor\";obs\n",
  res_get_handler,
  NULL,
  NULL,
  NULL,
  res_event_handler);


/*Change values function: the alarm is on with a probability equal to 0,1*/
static void
change(void){
  int change = (random_rand() % 101);
  if(change <= PROBABILITY_SOUND)
    alarm = true;
  else
    alarm = false;
}

/*Called by a timed periodic event*/
static void
res_event_handler(void){
  change();
  //if(alarm)
    //LOG_DBG("alarm: true\n");
  coap_notify_observers(&res_alarm_s);
}

/*
  * GET JSON payload:
  *   ""
  * Format of JSON response:
  * {"response_code":"1","alarm":"0|1"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);
  if(alarm)
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"alarm\":\"1\"}", RESPONSE_OK);
  else
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"alarm\":\"0\"}", RESPONSE_OK);

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
