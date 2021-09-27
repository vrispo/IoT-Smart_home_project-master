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

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

/* A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated */
EVENT_RESOURCE(res_roller_blind,
  "title=\"roller blind\";"
  "methods=\"GET/PUT/POST\";"
  "payload={\"mode\":\"on|off\",\"level\":\"0..100\"};"
  "rt=\"Control\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);

int actual_lvl = 0;
static int desired_lvl = 0;
static bool actual_mode = false;

#if PLATFORM_HAS_LEDS || LEDS_COUNT

/* Change values function: controlla se la tapparella è arrivata al livello desiderato e se non lo è aumenta o diminuisce il livello */
static void
change(void){
  if(actual_mode){
    if(actual_lvl < desired_lvl)
      actual_lvl++;
    if(actual_lvl > desired_lvl)
      actual_lvl--;
    if(actual_lvl == desired_lvl){
      LOG_DBG("** Roller Blind: desired level reached! **\n");
      actual_mode = false;
      leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
    }
  }else{
      leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
      leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
  }
}

/*Called by a timed periodic event*/
static void
res_event_handler(void){
  change();
  // Notify all the observers
  coap_notify_observers(&res_roller_blind);
}


/*
  * POST/PUT JSON payload:
  *   {"mode":"on|off","level":"0..100"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  char level[10];
  char mode[10];
  int success = 1;
  const uint8_t *payload = NULL;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"mode",mode,sizeof(char)*10);
    if(strcmp(mode, "") != 0){
      LOG_DBG("roller blind mode: %s\n", mode);

      if(strcmp(mode, "on") == 0) {
        actual_mode = true;

        json_decode(payload_s,"level",level,sizeof(char)*10);
        if(strcmp(level, "") != 0){
          LOG_DBG("desired level: %s\n",level);
          desired_lvl = atoi(level);
        }else{
          desired_lvl = 100;
        }
        leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      }else if(strcmp(mode, "off") == 0) {
        actual_mode = false;
        desired_lvl = actual_lvl;
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
        leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      }else{
        success = 0;
      }
    }else{ //non c'è key mode
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
  * {"response_code":"1","mode":"on|off","actual_level":"0..100","desired_level":"0..100"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);
  if(actual_mode)
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"mode\":\"on\",\"actual_level\":\"%d\",\"desired_level\":\"%d\"}", RESPONSE_OK, actual_lvl, desired_lvl);
  else
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"mode\":\"off\",\"actual_level\":\"%d\"}", RESPONSE_OK, actual_lvl);

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
