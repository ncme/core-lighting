/*
 * bbreg.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "coap.h"
#include "json.h"
#include "core-lighting.h"

#ifndef LIB_CL_INCLUDE_BULLETINS_H_
#define LIB_CL_INCLUDE_BULLETINS_H_

void bulletinboard_register(coap_context_t* context,
		const char* server_uri, char* base_uri,
		linkobj_node_t* links,
		thing_description_t* td);

#endif /* LIB_CL_INCLUDE_BULLETINS_H_ */
