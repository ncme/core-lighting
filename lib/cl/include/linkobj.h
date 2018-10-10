/*
 * linkobj.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */

#ifndef LIB_CL_INCLUDE_LINKOBJ_H_
#define LIB_CL_INCLUDE_LINKOBJ_H_

#include "json.h"

typedef struct linkobj {
	char* href;
	char* type;
} linkobj_t;

typedef struct linkobj_array {
	linkobj_t** lptr;
	size_t size;
} linkobj_array_t;

typedef struct linkobj_node {
	char* idf;
	json_type type;
	union {
		linkobj_t* link;
		linkobj_array_t* link_arr;
	};
	struct linkobj_node* next;
} linkobj_node_t;

void json_append_link_objects(linkobj_node_t* lnode, json_object* root);

int json_decode_link_objects(json_object* root, linkobj_node_t** node);

#endif /* LIB_CL_INCLUDE_LINKOBJ_H_ */
