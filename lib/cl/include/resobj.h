/*
 * resobj.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */

#ifndef LIB_CL_INCLUDE_RESOBJ_H_
#define LIB_CL_INCLUDE_RESOBJ_H_

#include "json.h"
#include "json-helper.h"
#include "linkobj.h"
#include "formobj.h"
#include "embdobj.h"
#include "content.h"

typedef struct resobj {
	char* base;
	linkobj_node_t* links;
	formobj_node_t* forms;
	embdobj_node_t* embedded;
	content_t* content;
} resobj_t;

void json_append_resourceobject(resobj_t* ro, json_object* root);

int json_decode_resourceobject(json_object* root, resobj_t* ro,
		content_type content_type);

void free_resourceobject(resobj_t* ro);

#endif /* LIB_CL_INCLUDE_RESOBJ_H_ */
