/*
 * json-helper.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#ifndef LIB_CL_INCLUDE_JSON_HELPER_H_
#define LIB_CL_INCLUDE_JSON_HELPER_H_

#include "json.h"
#include "coap.h"

#define JSON_GET_OBJ_OR_ERR(obj, key, value) if( !json_object_object_get_ex(obj, key, &value) ) return 0
#define JSON_GET_STRCPY_OR_ERR(obj, key, pstr) if( !json_get_str(obj, key, pstr) ) return 0
#define JSON_GET_STRING_OR_ERR(obj, key, pstr) *pstr = (char*) json_object_get_string(json_object_object_get(obj, key))
#define JSON_PUT_STR(obj, key, str) json_object_object_add(obj, key, json_object_new_string(str))

int json_get_str_cpy(json_object* obj, char* key, char** pstr);

const char* json_to_str(json_object* root);

json_object* string_to_json(unsigned char* data, size_t len);

#endif /* LIB_CL_INCLUDE_JSON_HELPER_H_ */
