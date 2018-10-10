/*
 * content.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */

#ifndef LIB_CL_INCLUDE_CONTENT_H_
#define LIB_CL_INCLUDE_CONTENT_H_

#include "json.h"

typedef enum content_type {
	CONTENT_TYPE_NONE = -1,
	APPLICATION_THINGDESCRIPTION_JSON = 65200,
	APPLICATION_BULLETIN_BOARD_JSON = 65201,
	APPLICATION_LIGHTING_CONFIG_JSON = 65202,
	APPLICATION_LIGHTING_JSON = 65203
} content_type;

typedef struct lighting {
	int on;
	double brightness;
	char* colorMode;
	int hue;
	double saturation;
	double colorTemperature;
	double x;
	double y;
} lighting_t;

typedef struct lighting_config {
	char* href;
} lighting_config_t;

typedef struct thing_description {
	char* name;
	char* purpose;
	char* location;
} thing_description_t;

typedef struct content {
	content_type type;
	union {
		lighting_t* lighting;
		lighting_config_t* config;
		thing_description_t* td;
	};
} content_t;

void json_encode_lightingconfig(lighting_config_t* config,
		json_object* root);

int json_decode_lightingconfig(json_object* root,
		lighting_config_t* config);

void json_encode_lighting(lighting_t* conf, json_object* root);

int json_decode_lighting(json_object* root, lighting_t* conf);

int json_decode_thingdescription(json_object* root,
		thing_description_t* td);

void json_encode_thingdescription(thing_description_t* td,
		json_object* root);

thing_description_t* new_thing_description(char* name, char* purpose,
		char* location);

void free_thing_description(thing_description_t* td);

#endif /* LIB_CL_INCLUDE_CONTENT_H_ */
