/*
 * embdobj.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */

#ifndef LIB_CL_INCLUDE_EMBDOBJ_H_
#define LIB_CL_INCLUDE_EMBDOBJ_H_

#include "json.h"
#include "json-helper.h"

typedef struct resobj_array {
	json_object** roptr;
	size_t size;
} resobj_array_t;

typedef struct embdobj_node {
	char* idf;
	json_type type;
	union {
		json_object* embedded;
		resobj_array_t* embedded_arr;
	};
	struct embdobj_node* next;
} embdobj_node_t;

void json_append_embedded_objects(embdobj_node_t* enode, json_object* root);

int json_decode_embedded_objects(json_object* root, embdobj_node_t** enode);

#endif /* LIB_CL_INCLUDE_EMBDOBJ_H_ */
