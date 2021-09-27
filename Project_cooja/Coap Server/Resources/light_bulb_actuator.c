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

#define LIGHT_ON_VALUE 40
#define OFF 0
#define ON 1
#define M_AUTO 2

extern int actual_status_light_sensors[MAX_SENSORS];
extern int n_light_sensor;
extern int actual_status_presence_sensors[MAX_SENSORS];
extern int n_presence_sensor;

static int actual_lvl = 0;
static int desired_lvl = 0;
int actual_light_bulb_mode = OFF; // 0->off 1->on 2->auto
static char actual_light[2] = "y";

static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler(void);

/* A simple actuator example, depending on the color query parameter and post variable mode, corresponding led is activated or deactivated */
EVENT_RESOURCE(res_light_bulb,
  "title=\"light bulb actuator\";"
  "methods=\"GET/PUT/POST\";"
  "payload={\"mode\":\"on|auto|off\",\"color\":\"y|g|r\",\"brightness\":\"0..100\"};"
  "rt=\"Control\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);

#if PLATFORM_HAS_LEDS || LEDS_COUNT

/* Change values function: Controlla se la luminosità è arrivata al giusto livello e nel caso l'aumenta o diminuisce*/
static void
change(void){
  if(actual_light_bulb_mode == ON){
    if(actual_lvl < desired_lvl)
      actual_lvl++;
    if(actual_lvl > desired_lvl)
      actual_lvl--;
    if(strcmp(actual_light,"y") == 0){
      leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
    }
    if(strcmp(actual_light,"r") == 0){
      leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
    }
    if(strcmp(actual_light,"g") == 0){
      leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
    }
  }
  if(actual_light_bulb_mode == M_AUTO){
    //accende led a seconda della luce e della presenza nella stanza
    bool on = false;
    int bright = 0;

    for(int i = 0; i < n_presence_sensor ; i++){
      if(actual_status_presence_sensors[i])
        on = true;
    }

    if(n_presence_sensor == 0){
      on = true;
    }

    if(on){
      for(int i = 0; i < n_light_sensor ; i++){
        bright +=actual_status_light_sensors[i];
      }

      bright = bright/n_light_sensor;

      if(n_light_sensor == 0){
        bright = 0;
      }

      if(bright >= 40)
        on = false;
    }
    if(on){
      if(bright < 10)
        desired_lvl = 100;
      if((bright >= 10)&&(bright < 20))
        desired_lvl = 75;
      if((bright >= 20)&&(bright < 30))
        desired_lvl = 50;
      if((bright >= 30)&&(bright <= 40))
        desired_lvl = 25;
      if(actual_lvl < desired_lvl)
        actual_lvl++;
      if(actual_lvl > desired_lvl)
        actual_lvl--;
      if(strcmp(actual_light,"y") == 0){
        leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
        leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      }
      if(strcmp(actual_light,"r") == 0){
        leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
        leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
      }
      if(strcmp(actual_light,"g") == 0){
        leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
        leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      }
    }else{
      desired_lvl = 0;
      if(actual_lvl > desired_lvl)
        actual_lvl--;
      leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
    }
    if(desired_lvl != actual_lvl)
      LOG_DBG("AUTO MODE desired brightness: %d\n", desired_lvl);
  }
}

/*Called by a timed periodic event*/
static void
res_event_handler(void){
  change();
  // Notify all the observers
  coap_notify_observers(&res_light_bulb);
}


/*
  * POST/PUT JSON payload:
  *   {"mode":"on|auto|off","color":"y|g|r","brightness":"0..100"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  char brightness[10];
  char mode[10];
  char color[3];
  int success = 1;
  const uint8_t *payload = NULL;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"mode",mode,sizeof(char)*10);
    if(strcmp(mode, "") != 0){
      LOG_DBG("light bulb mode: %s\n", mode);

      if(strcmp(mode, "on") == 0) {
        actual_light_bulb_mode = ON;
        json_decode(payload_s,"brightness",brightness,sizeof(char)*10);
        if(strcmp(brightness, "") != 0){
          LOG_DBG("desired brightness: %s\n",  brightness);
          desired_lvl = atoi(brightness);
        }else{
          LOG_DBG("default brightness: %s\n",  brightness);
          desired_lvl = 50;
        }
        json_decode(payload_s,"color",color,sizeof(char)*3);
        if(strcmp(color, "") != 0){
          LOG_DBG("desired color: %s\n",color);

          if(strncmp(color, "y", len) == 0) {
            strcpy(actual_light,color);
            leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
            leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
            leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
          } else if(strncmp(color, "g", len) == 0) {
            strcpy(actual_light,color);
            leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
            leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
            leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
          } else if(strncmp(color, "r", len) == 0) {
            strcpy(actual_light,color);
            leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
            leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
            leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
          } else {
            success = 0;
          }
        }else{ //manca la key colore
          success = 0;
        }
      }else{
        if(strncmp(mode, "off", len) == 0) {
          actual_light_bulb_mode = OFF;
          desired_lvl = actual_lvl;
          leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
          leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
          leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
          }else{
            if(strncmp(mode, "auto", len) == 0) {
              actual_light_bulb_mode = M_AUTO;
              desired_lvl = actual_lvl;
              leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
              leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
              leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
            }else{ //value of key mode errato
              success = 0;
            }
          }
        }
      }else{ // key mode non presente
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
  * {"response_code":"1","mode":"on|auto|off","brightness":"0..100", "color":"y|r|g"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);
  if(actual_light_bulb_mode == ON)
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE,"{\"response_code\":\"%d\",\"mode\":\"on\",\"brightness\":\"%d\",\"color\":\"%s\"}", RESPONSE_OK, actual_lvl, actual_light);
  if(actual_light_bulb_mode == M_AUTO)
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"mode\":\"auto\",\"brightness\":\"%d\",\"color\":\"%s\"}", RESPONSE_OK, actual_lvl, actual_light);
    if(actual_light_bulb_mode == OFF)
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE,"{\"response_code\":\"%d\",\"mode\":\"off\",\"brightness\":\"%d\",\"color\":\"%s\"}", RESPONSE_OK, actual_lvl, actual_light);

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
