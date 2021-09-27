#include "contiki.h"
#include "coap-engine.h"
#include "dev/leds.h"
#include <stdbool.h>
#include <stdlib.h>

#include <string.h>

#include "json.h"
#include "utilities.h"

#define DEFAULT_TEMP 20
#define DEFAULT_HUM 15
#define PROGR_HOT 0
#define PROGR_COLD 1
#define PROGR_HUM_PLUS 2
#define PROGR_HUM_MINUS 3

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP


extern int actual_status_temp_hum_sensors[MAX_SENSORS][2];
extern int n_temp_hum_sensor;

static int desired_hum = DEFAULT_HUM;
static int desired_temp = DEFAULT_TEMP;
static int actual_hum = 0;
static int actual_temp = 0;
bool actual_mode_hvac = false;
int program[4] = {0,0,0,0};

/* Handler declaration */
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler();

EVENT_RESOURCE(res_hvac,
  "title=\"hvac\";"
  "methods=\"GET/PUT/POST\";"
  "payload={\"mode\":\"on|off\",\"temp\":\"-10..40\",\"hum\":\"0..100\"};"
  "rt=\"Control\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);

#if PLATFORM_HAS_LEDS || LEDS_COUNT

/*
  * Se è acceso:
  *   controlla i sensori associati per vedere la temperatura ed umidità attuali ed a seconda che siano più alti o più bassi di quelli desiderati
  *   ed in base a questo attiva il programma adeguato o si ferma se siamo arrivati ai valori desiderati
  *
  *  Accende i led in modo adeguato a seconda della situazione:
  *   actual_mode_hvac = OFF -> LEDS_RED
  *   actual_mode_hvac = ON  -> se valori desiderati  =  valori attuali: LEDS_GREEN
  *                     -> se valori desiderati !=  valori attuali: LED_YELLOW
*/
static void
change(void){

  for(int i = 0; i<4 ; i++)
    program[i] = 0;

  if(actual_mode_hvac){
    actual_hum = 0;
    actual_temp = 0;
    for(int i = 0 ; i < n_temp_hum_sensor ; i++){
      actual_temp += actual_status_temp_hum_sensors[i][0];
      actual_hum += actual_status_temp_hum_sensors[i][1];
    }
    actual_temp = actual_temp / n_temp_hum_sensor;
    actual_hum = actual_hum / n_temp_hum_sensor;
    //LOG_DBG("Media dati stanza: %d %d - ", actual_temp, actual_hum);

    if(actual_hum < desired_hum)
      program[PROGR_HUM_PLUS] = 1;
    if(actual_hum > desired_hum)
      program[PROGR_HUM_MINUS] = 1;
    if(actual_temp < desired_temp)
      program[PROGR_HOT] = 1;
    if(actual_temp > desired_temp)
      program[PROGR_COLD] = 1;
    if((actual_temp == desired_temp)&&(actual_hum == desired_hum)){
      actual_mode_hvac = false;
      LOG_DBG("** Desired temperature and humidity reached! **\n");
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
res_event_handler(){
  change();
  // Notify all the observers
  coap_notify_observers(&res_hvac);
}

/*
  * POST/PUT JSON payload:
  *   {"mode":"on|off","temp":"value","hum":"value"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  const uint8_t *payload = NULL;

  char temp[10];
  char mode[4];
  char hum[10];
  int success = 1;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"mode",mode,sizeof(char)*4);

    if(strcmp(mode, "") != 0){
      LOG_DBG("hvac mode: %s\n", mode);

      if(strcmp(mode, "on") == 0) {
        actual_mode_hvac = true;

        json_decode(payload_s,"hum",hum,sizeof(char)*10);
        if(strcmp(hum, "") != 0){
          LOG_DBG("desired hum: %s\n", hum);
          desired_hum = atoi(hum);
        }else
          desired_hum = DEFAULT_HUM;

        json_decode(payload_s,"temp",temp,sizeof(char)*10);
        if(strcmp(temp, "") != 0){
          LOG_DBG("desired temp: %s\n", temp);
          desired_temp = atoi(temp);
        }else
          desired_temp = DEFAULT_TEMP;

        leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
        leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      }else if(strcmp(mode, "off") == 0) {
        actual_mode_hvac = false;
        desired_temp = DEFAULT_TEMP;
        desired_hum = DEFAULT_HUM;
        leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
        leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
        leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
      }else{ // value of mode errato
        success = 0;
      }
    }else{ // key mode non presente
      success = 0;
    }
  }else{ // nessun payload presente
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
  *   "" to get all the value
  *   {"value":"temp|hum"}
  * Format of JSON response:
  *   {"response_code":"1","mode":"on|off","des_temp":"int","prog_temp":"string","des_hum":"int","prog_hum":"string"}
  *   {"response_code":"1","mode":"on|off","des_temp":"int","prog_temp":"string"}
  *   {"response_code":"1","mode":"on|off","des_hum":"int","prog_hum":"string"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);

  const uint8_t *payload = NULL;
  char payload_s[200];
  char value[10];

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    sprintf(payload_s,"%s",(char*)payload);

    json_decode(payload_s,"value",value,sizeof(char)*10);

    if(strcmp(value, "") != 0){
      if(strncmp(value, "temp",1) == 0){
        if(program[PROGR_HOT])
          snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"HOT\"}", desired_temp);
        else{
          if(program[PROGR_COLD])
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"COLD\"}", desired_temp);
          else{
            if(actual_mode_hvac)
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE,  "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"NONE\"}", desired_temp);
            else
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE,  "{\"response_code\":\"1\",\"mode\":\"off\",\"des_temp\":\"%d\",\"prog_temp\":\"NONE\"}", desired_temp);
          }
        }
      }else{
        if(strncmp(value, "hum",1) == 0){
          if(program[PROGR_HUM_PLUS])
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_hum\":\"%d\",\"prog_hum\":\"PLUS\"}", desired_hum);
          else{
            if(program[PROGR_HUM_MINUS])
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_hum\":\"%d\",\"prog_hum\":\"MINUS\"}", desired_hum);
            else{
              if(actual_mode_hvac)
                snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_hum\":\"%d\",\"prog_hum\":\"NONE\"}", desired_hum);
              else
                snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"off\",\"des_hum\":\"%d\",\"prog_hum\":\"NONE\"}", desired_hum);
            }
          }
        }
        else //not valid payload -> value non è ne temp ne hum
          snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"-1\"}");
      }
    }else{ // not valid payload -> key sbagliata
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"-1\"}");
    }
  }else{ // paylod vuoto -> GET all values
    if(actual_mode_hvac){
      if(program[PROGR_HOT]){
        if(program[PROGR_HUM_PLUS])
          snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"HOT\",\"des_hum\":\"%d\",\"prog_hum\":\"PLUS\"}", desired_temp, desired_hum);
        else{
          if(program[PROGR_HUM_MINUS])
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"HOT\",\"des_hum\":\"%d\",\"prog_hum\":\"MINUS\"}", desired_temp, desired_hum);
          else //prog_hum=NONE
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"HOT\",\"des_hum\":\"%d\",\"prog_hum\":\"NONE\"}", desired_temp, desired_hum);
        }
      }else{
        if(program[PROGR_COLD]){
          if(program[PROGR_HUM_PLUS])
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"COLD\",\"des_hum\":\"%d\",\"prog_hum\":\"PLUS\"}", desired_temp, desired_hum);
          else{
            if(program[PROGR_HUM_MINUS])
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"COLD\",\"des_hum\":\"%d\",\"prog_hum\":\"MINUS\"}", desired_temp, desired_hum);
            else //prog_hum=NONE
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"COLD\",\"des_hum\":\"%d\",\"prog_hum\":\"NONE\"}", desired_temp, desired_hum);
          }
        }else{ //prog_temp=NONE
          if(program[PROGR_HUM_PLUS])
            snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"NONE\",\"des_hum\":\"%d\",\"prog_hum\":\"PLUS\"}", desired_temp, desired_hum);
          else{
            if(program[PROGR_HUM_MINUS])
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"NONE\",\"des_hum\":\"%d\",\"prog_hum\":\"MINUS\"}", desired_temp, desired_hum);
            else //prog_hum=NONE
              snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"on\",\"des_temp\":\"%d\",\"prog_temp\":\"NONE\",\"des_hum\":\"%d\",\"prog_hum\":\"NONE\"}", desired_temp, desired_hum);
          }
        }
      }
    }
    else{
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"1\",\"mode\":\"off\",\"des_temp\":\"%d\",\"prog_temp\":\"NONE\",\"des_hum\":\"%d\",\"prog_hum\":\"NONE\"}", desired_temp, desired_hum);
    }
  }

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
