/*
 * core-lighting.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#ifndef CORE_LIGHTING_H_
#define CORE_LIGHTING_H_

#include "json.h"
#include "resobj.h"

size_t create_response(resobj_t* ro, unsigned char** response);

int parse_response(json_object* root, resobj_t* ro,
		content_type content_type);

#endif /* CORE_LIGHTING_H_ */
