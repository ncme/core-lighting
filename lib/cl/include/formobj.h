/*
 * formobj.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#ifndef LIB_CL_INCLUDE_FORMOBJ_H_
#define LIB_CL_INCLUDE_FORMOBJ_H_

#include "json.h"

typedef struct formobj {
	char* href;
	char* method;
	char* accept;
} formobj_t;

typedef struct formobj_array {
	formobj_t** foptr;
	size_t size;
} formobj_array_t;

typedef struct formobj_node {
	char* idf;
	json_type type;
	union {
		formobj_t* form;
		formobj_array_t* form_arr;
	};
	struct formobj_node* next;
} formobj_node_t;

void json_append_form_objects(formobj_node_t* fnode, json_object* root);

int json_decode_form_objects(json_object* root, formobj_node_t** fnode);

#endif /* LIB_CL_INCLUDE_FORMOBJ_H_ */
