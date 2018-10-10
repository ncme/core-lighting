/*
 * json-helper.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "json.h"
#include "json-helper.h"
#include <string.h>

/**
 * Creates a heap allocated copy of the string provided by
 * searching for 'key' within 'obj' at 'pstr'.
 *
 * Returns 1 on success, 0 if 'key' was not found in 'obj'
 */
int json_get_str_cpy(json_object* obj, char* key, char** pstr) {
	json_object* tmp = NULL;
	if( !json_object_object_get_ex(obj, key, &tmp) )
		return 0;
	const char* ret = json_object_get_string(tmp);
	size_t size = strlen(ret);
	char* val = (char*) malloc((size+1) * sizeof(char));
	memcpy(val,ret,size);
	val[size] = '\0';
	*pstr = val;
	return 1;
}

const char* json_to_str(json_object* root) {
	return json_object_to_json_string_ext(root, JSON_C_TO_STRING_NOSLASHESCAPE);
}
/*
 * Parse string without use of strlen (can handle strings that are not null terminated)
 */
json_object* string_to_json(unsigned char* data, size_t len) {
    struct json_tokener* tok;
    struct json_object* obj;

    tok = json_tokener_new();
    if (!tok)
      return NULL;
    obj = json_tokener_parse_ex(tok, (char*) data, len);
    if(tok->err != json_tokener_success) {
		if (obj != NULL)
			json_object_put(obj);
        obj = NULL;
    }
    json_tokener_free(tok);
    return obj;
}
