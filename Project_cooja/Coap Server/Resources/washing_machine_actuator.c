#include "contiki.h"
#include "coap-engine.h"
#include "dev/leds.h"
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#include "json.h"
#include "utilities.h"

#define FREE 0
#define WORKING 1
#define START 1
#define PAUSE 2

/* Log configuration */
#include "sys/log.h"
#define LOG_MODULE "App"
#define LOG_LEVEL LOG_LEVEL_APP


/* Handler declaration */
static void res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_event_handler();

EVENT_RESOURCE(res_washing_m,
  "title=\"washing machine actuator\";"
  "methods=\"GET/PUT/POST\";"
  "payload={\"mode\":\"start|pause\",\"program\":\"0|1|2\",\"centrifuge\":\"0|1|2\",\"status\":\"free|work\"};"
  "rt=\"Control\";obs\n",
  res_get_handler,
  res_post_put_handler,
  res_post_put_handler,
  NULL,
  res_event_handler);

static int actual_status = FREE;
static int actual_mode = FREE;
static int program = 0;
static int centrifuge = 0;
static time_t start_t;
// static int t_program[3] = {2700, 3600,4500}; tempi realistici
static int t_program[3] = {60, 90, 120}; //tempi simulazione
// static int t_centr[3] = {0, 600, 900}; tempi realistici
static int t_pcentr[3] = {0, 10, 15}; //tempi simulazione
static double sec_t = 0;
static int sec_r = 0;
static int sec_pause = 0;


#if PLATFORM_HAS_LEDS || LEDS_COUNT

/*
  * Controlla se un lavaggio è in corso e nel caso controlla se è finita ed aggiorna il tempo restante
  *
  * Accende i led in modo adeguato a seconda della situazione:
  *   actual_mode = FREE -> LEDS_GREEN
  *   actual_mode = WORKING -> se actual_status = pause: LEDS_RED
  *                         -> se actual_status = start: LED_YELLOW
*/
static void
change(void){
  if(actual_mode == START){
    sec_t = difftime(time(NULL), start_t);
    sec_r = t_program[program] + t_pcentr[centrifuge]- (int)sec_t - sec_pause;
    /* Controllo se ha finito e nel caso resetto tutti i valori */
    if(sec_r <= 0){
      LOG_DBG("** washing machine: time finished **\n");
      actual_status = FREE;
      actual_mode = FREE;
      program = 0;
      centrifuge = 0;
      sec_t = 0;
      sec_r = 0;
      sec_pause = 0;
      leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
      leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
      leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
    }
  }
  if(actual_status == FREE){
    leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
    leds_on(LEDS_NUM_TO_MASK(LEDS_GREEN));
    leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
  }
}

/*Called by a timed periodic event*/
static void
res_event_handler(){
  change();
  //if(actual_status == WORKING){
  //  LOG_DBG("Remaining time= %d ", sec_r);
  //}
  // Notify all the observers
  coap_notify_observers(&res_washing_m);
}

/*
  * POST/PUT JSON payload:
  *   {"mode":"start|pause","program":"0|1|2","centrifuge":"0|1|2"}
*/
static void
res_post_put_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  char prg[10];
  char cntr[10];
  char mode[10];
  int success = 1;
  const uint8_t *payload = NULL;

  coap_set_header_content_format(response, APPLICATION_JSON);

  int len = coap_get_payload(request, &payload);
  if(len > 1){
    char payload_s[200];
    sprintf(payload_s,"%s",(char*)payload);

    if(actual_status == FREE){
      json_decode(payload_s,"mode",mode,sizeof(char)*10);
      if(strcmp(mode, "") != 0){
        LOG_DBG("mode %s\n", mode);
        if(strcmp(mode, "start") == 0) {
          json_decode(payload_s,"program",prg,sizeof(char)*10);
          if(strcmp(prg, "") != 0){
            LOG_DBG("program %s\n", prg);
            program = atoi(prg);
          }
          json_decode(payload_s,"centrifuge",cntr,sizeof(char)*10);
          if(strcmp(cntr, "") != 0){
            LOG_DBG("centrifuge %s\n", cntr);
            centrifuge = atoi(cntr);
          }
          actual_status = WORKING;
          actual_mode = START;
          start_t = time(NULL);
          sec_r = t_program[program];
          leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
          leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
          leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
          LOG_DBG("** washing machine: Start working, time: %d **\n",sec_r);
        }else{
          success = 0;
        }
      }
    }else{
      if(actual_mode == START){
        json_decode(payload_s,"mode",mode,sizeof(char)*10);
        if(strcmp(mode, "") != 0){
          if(strcmp(mode, "pause") == 0) {
            LOG_DBG(" ** washing machine: %s **\n", mode);
            sec_pause = sec_pause + difftime(time(NULL), start_t);
            actual_mode = PAUSE;
            leds_off(LEDS_NUM_TO_MASK(LEDS_YELLOW));
            leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
            leds_on(LEDS_NUM_TO_MASK(LEDS_RED));
          }
        }
      }else{
        json_decode(payload_s,"mode",mode,sizeof(char)*10);
        if(strcmp(mode, "") != 0){
          if(strcmp(mode, "start") == 0) {
            LOG_DBG(" ** washing machine: re %s **\n", mode);
            start_t = time(NULL);
            actual_mode = START;
            leds_on(LEDS_NUM_TO_MASK(LEDS_YELLOW));
            leds_off(LEDS_NUM_TO_MASK(LEDS_GREEN));
            leds_off(LEDS_NUM_TO_MASK(LEDS_RED));
          }
        }
      }
    }
  }else{ // se non c'è payload
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
  * {"response_code":"1","status":"start|free","mode":"start|pause|free","program":"0|1|2","centrifuge":"0|1|2", "r_time":"0..135"}
*/
static void
res_get_handler(coap_message_t *request, coap_message_t *response, uint8_t *buffer, uint16_t preferred_size, int32_t *offset){
  coap_set_header_content_format(response, APPLICATION_JSON);

  if(actual_status){
    if(actual_mode == FREE){
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"status\":\"working\",\"mode\":\"free\",\"program\":\"%d\",\"centrifuge\":\"%d\",\"r_time\":\"%d\"}", RESPONSE_OK, program, centrifuge, sec_r);
    }
    if(actual_mode == START){
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"status\":\"working\",\"mode\":\"start\",\"program\":\"%d\",\"centrifuge\":\"%d\",\"r_time\":\"%d\"}", RESPONSE_OK, program, centrifuge, sec_r);
    }
    if(actual_mode == PAUSE){
      snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"status\":\"working\",\"mode\":\"pause\",\"program\":\"%d\",\"centrifuge\":\"%d\",\"r_time\":\"%d\"}", RESPONSE_OK, program, centrifuge, sec_r);
    }
  }
  else
    snprintf((char *)buffer, COAP_MAX_CHUNK_SIZE, "{\"response_code\":\"%d\",\"status\":\"free\"}", RESPONSE_OK);

  coap_set_payload(response, (uint8_t *)buffer, strlen((char *)buffer));
}
