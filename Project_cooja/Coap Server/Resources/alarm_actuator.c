#include "contiki.h"
#include "coap-engine.h"
#include "dev/leds.h"
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>

#include "json.h"
#include "utilities.h"

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP

#define OK 0
#define SOUNDING 1

#define OFF 0
#define ON 1

extern int actual_status_alarm_sensors[MAX_SENSORS];
extern int n_alarm_sensor;

static int actual_status = OK;
bool actual_alarm_act_mode = OFF;

/* Handler declaration */
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler();

EVENT_RESOURCE(res_alarm_act,
  "title=\"alarm actuator\";"
  "methods=\"GET/PUT/POST\";"
  "payload={\"mode\":\"on|off\",\"status\":\"ok|sound\"};"
  "rt=\"Control\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);


#if PLATFORM_HAS_LEDS || LEDS_COUNT

/*
  * Controlla:
  *   nel caso sia acceso (actual_alarm_act_mode=ON) se uno dei sensori associati a questo attuatore ha rilevato qualcosa ed entra in tal caso nello stato SOUNDING
  *   nel caso sia spento non fa nulla
  * Accende i led in modo adeguato a seconda della situazione:
  *   actual_alarm_act_mode = OFF -> LEDS_RED
  *   actual_alarm_act_mode = ON  -> se actual_status = OK:       LEDS_GREEN
  *                     -> se actual_status = SOUNDING: LED_YELLOW, LEDS_RED
  *
  * NOTA: Se sta suonando continua comunque a suonare finchè non viene spento
*/
static void
change(void){
  if(actual_alarm_act_mode){
    for(int i = 0 ; i < n_alarm_sensor ; i++){
      if(actual_status_alarm_sensors[i]){
        actual_status = SOUNDING;
      }
    }
    if(actual_status){
      leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
      leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
    }
  }else{
    leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
    leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
    leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
  }
}

/* Called by a timed periodic event */
static void
res_event_handler(){
  change();
  if(actual_alarm_act_mode){
    if(actual_status)
      LOG_DBG("** ALARM SOUNDS **\n");
  }
  // Notify all the observers
  coap_notify_observers(&res_alarm_act);
}

/*
  * POST/PUT JSON payload:
  *   {"mode":"on|off"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  char mode[4];
  int success = 1;

  const uint8_t *payload = NULL;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"mode",mode,sizeof(char)*4);

    if(strcmp(mode, "") != 0){
      LOG_DBG("mode %s\n", mode);
      if(strcmp(mode, "on") == 0) {
        actual_alarm_act_mode = ON;
        leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
        leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      }else if(strcmp(mode, "off") == 0) {
        actual_alarm_act_mode = OFF;
        actual_status = OK;
        leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
        leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      }else{ // value di mode diverso da on o off
        success = 0 ;
      }
    }else{ //key mode non presente
      success = 0;
    }
  }else{ //payload non presente
    success = 0;
  }

  if(!success) {
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\"}", RESPONSE_BAD);
  }else{
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\"}", RESPONSE_OK);
  }
  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}

#endif /* PLATFORM_HAS_LEDS */

/*
  * GET JSON payload:
  *   ""
  * Format of JSON response:
  * {"response_code":"1","mode":"on|off","status":"ok|sound"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);

  if(actual_alarm_act_mode){
    if(actual_status)
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"mode\":\"on\",\"status\":\"sound\"}", RESPONSE_OK);
    else
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"mode\":\"on\",\"status\":\"ok\"}", RESPONSE_OK);
  }else
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"mode\":\"off\",\"status\":\"ok\"}", RESPONSE_OK);

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
