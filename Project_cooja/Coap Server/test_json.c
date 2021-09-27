
#include<stdio.h>
#include "json.h"

int main(){

    char json[400] = "{\"ip\" : \"127.0.0.1\", \"room\" : \"bagno\" , \"gerlando\" : \"gay\"}";

    char pippo[100];
    json_decode(json,"gerlando",pippo);
    printf("Gerlando --> %s\n", pippo);
    json_decode(json,"garavagno",pippo);
    printf("Test error --> %s  <-- correttamente vuota\n", pippo);
    printf("----------------------------\n");
    
    char pippo2[1000] = {0};
    float a   = 4.2; 
    int b     = 2;
    void *pr  = &a;
    void *pr2 = &b;
    char *t   = "jerry";
    void *pr3 = t;
    
    to_json(pippo2,"prova",pr, FLOAT);
    to_json(pippo2,"test",pr2, INT);
    to_json(pippo2,"stringa",pr3, STRING);
    dump_json_buffer(pippo2);
    clear_json_buffer(pippo2);
    dump_json_buffer(pippo2);
    to_json(pippo2,"stringa",pr3, STRING);
    dump_json_buffer(pippo2);
    clear_json_buffer(pippo2);
    to_json(pippo2,"stringa",pr3, STRING);
    dump_json_buffer(pippo2);
    return 0;
}