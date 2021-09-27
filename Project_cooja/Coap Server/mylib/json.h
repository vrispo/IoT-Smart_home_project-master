
#ifndef __JSON_H__
#define __JSON_H__

enum data_type{ INT, FLOAT, STRING};


int  match(const char *text, const char *pattern);
void json_decode(const char *data, char *key, char *value, size_t size_value);
void to_json(char *buffer, char *key, void *value, int type);
void clear_json_buffer(char *json_buffer, size_t size_json_buffer);
void dump_json_buffer(char *json_buffer);

#endif