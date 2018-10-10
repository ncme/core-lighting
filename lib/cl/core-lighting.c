/*
 * core-lighting.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "core-lighting.h"
#include "resobj.h"
#include "json.h"

#include <string.h>

size_t create_response(resobj_t* ro, unsigned char** response) {
	json_object* root = json_object_new_object();
	json_append_resourceobject(ro, root);
	const char* json_str = json_to_str(root);
	size_t len = strlen(json_str);
	*response = malloc(len + 1);
	strcpy((char*) *response, json_str);
	json_object_put(root); /* free json memory */
	return len;
}

int parse_response(json_object* root, resobj_t* ro, content_type content_type) {
	return json_decode_resourceobject(root, ro, content_type);
}







