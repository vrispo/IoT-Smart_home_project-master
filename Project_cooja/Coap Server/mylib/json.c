
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include "json.h"
#include <stdint.h>


/* Algoritmo di Pattern matching KMP, ci serve per trovare la chiave *
 * desiderata, ovvero una sottostringa nella stringa del json totale */
int match(const char *text, const char *pattern){
  int c, d, e, text_length, pattern_length, position = -1;

  text_length    = strlen(text);
  pattern_length = strlen(pattern);

  if(pattern_length > text_length){ return -1; }

  for(c = 0; c <= text_length - pattern_length; c++){
    position = e = c;

    for(d = 0; d < pattern_length; d++){
      if(pattern[d] == text[e]){
        e++;
      }
      else{
        break;
      }
    }
    if (d == pattern_length) {
      return position;
    }
  }
  return -1;
}


/* Restituisce il valore nel buffer value corrispondente alla chiave Key passata *
 * nella stringa json data... se la chiave non è presente viene restituito ""    */
void json_decode(const char *data, char *key, char *value, size_t size_value){
    //int payload_size = strlen(key);
    int idx          = 0;
    bool copy        = false;

    char temp[2]     = {0};

    temp[1] = '\0';
    memset(value, 0, sizeof(char) * size_value);
    idx = match(data, key);
    if(idx != -1){
        while(data[idx] != ':'){
            idx += 1;
        }

        while(1){
            if((data[idx] == '"' || data[idx] == '\'') && copy == false){ copy = true; }
            idx += 1;
            if((data[idx] == '"' || data[idx] == '\'') && copy == true){ break; }
            if(copy == true && data[idx] != ' ' && data[idx] != '"'){
                temp[0] = data[idx];
                strncat(value, temp,(size_t)1);
            }
        }
    }
}


//aggiunge una coppia chiave valore al json buffer "buffer", con la chiave key
void to_json(char *buffer, char *key, void *value, int type){
    char temp[100] = {0};
    int i          = 0;

    if(buffer[0] == '{'){
        for(i = 0; i < strlen(buffer); i++){
            if(buffer[i] == '}'){
                buffer[i] = ',';
                break;
            }
        }
    }else{
        strcat(buffer,"{");
    }

    if(type == INT){
        int parsed_val = *((intptr_t *)value);
        sprintf(temp, " \"%s\" : \"%d\"", key, parsed_val);
    }

    if(type == FLOAT){
        float parsed_val = *((float*)value);
        sprintf(temp, " \"%s\" : \"%f\"", key, parsed_val);
    }

    if(type == STRING){
        char *parsed_val = (char *)value;
        sprintf(temp, " \"%s\" : \"%s\"", key, parsed_val);
    }

    strcat(buffer, temp);
    strcat(buffer," }");
}




void clear_json_buffer(char *json_buffer, size_t size_json_buffer){
    memset(json_buffer, 0, sizeof(char) *size_json_buffer);
}


void dump_json_buffer(char *json_buffer){
    printf("%s\n", json_buffer);
}
