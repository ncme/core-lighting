/*
 * content.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "content.h"
#include "json-helper.h"

void json_encode_lightingconfig(lighting_config_t* config, json_object* root) {
	json_object* src = json_object_new_object();

	JSON_PUT_STR(src, "href", config->href);

	json_object_object_add(root, "src", src);
}

int json_decode_lightingconfig(json_object* root, lighting_config_t* config) {
	json_object* src = NULL;
	JSON_GET_OBJ_OR_ERR(root, "src", src);
	JSON_GET_STRING_OR_ERR(src, "href", &config->href);
	return 1;
}

void json_encode_lighting(lighting_t* state, json_object* root) {
	json_object* on = json_object_new_boolean((json_bool) state->on);
	json_object* brightness = json_object_new_double(state->brightness);
	json_object* colorMode = json_object_new_string((const char*) state->colorMode);
	json_object* hue = json_object_new_int(state->hue);
	json_object* saturation = json_object_new_double(state->saturation);
	json_object* colorTemperature = json_object_new_int(state->colorTemperature);
	json_object* x = json_object_new_double(state->x);
	json_object* y = json_object_new_double(state->y);

	json_object_object_add(root, "on", on);
	json_object_object_add(root, "brightness", brightness);
	json_object_object_add(root, "colorMode", colorMode);
	json_object_object_add(root, "hue", hue);
	json_object_object_add(root, "saturation", saturation);
	json_object_object_add(root, "colorTemperature", colorTemperature);
	json_object_object_add(root, "x", x);
	json_object_object_add(root, "y", y);
}

int json_decode_lighting(json_object* root, lighting_t* state) {
	json_bool on = json_object_get_boolean(
			json_object_object_get(root, "on"));
	double brightness = json_object_get_double(
			json_object_object_get(root, "brightness"));
	const char* colorMode = json_object_get_string(
			json_object_object_get(root, "colorMode"));
	int hue = json_object_get_int(
			json_object_object_get(root, "hue"));
	double saturation = json_object_get_double(
			json_object_object_get(root, "saturation"));
	int colorTemperature = json_object_get_int(
				json_object_object_get(root, "colorTemperature"));
	double x = json_object_get_double(
			json_object_object_get(root, "x"));
	double y = json_object_get_double(
			json_object_object_get(root, "y"));
	*state = (lighting_t ) {
		.on = (int) on,
		.brightness = brightness,
		.colorMode = (char*) colorMode,
		.hue = hue,
		.saturation = saturation,
		.colorTemperature = colorTemperature,
		.x = x,
		.y = y
	};
	return 1;
}

void json_encode_thingdescription(thing_description_t* td, json_object* root) {
	JSON_PUT_STR(root, "name", td->name);
	JSON_PUT_STR(root, "purpose", td->purpose);
	JSON_PUT_STR(root, "location", td->location);
}

int json_decode_thingdescription(json_object* root, thing_description_t* td) {
	JSON_GET_STRING_OR_ERR(root, "name", &td->name);
	JSON_GET_STRING_OR_ERR(root, "purpose", &td->purpose);
	JSON_GET_STRING_OR_ERR(root, "location", &td->location);
	return 1;
}

thing_description_t* new_thing_description(char* name, char* purpose, char* location) {
	thing_description_t* td = malloc(sizeof(thing_description_t));
	if (td && name && purpose && location) {
		td->name = name;
		td->purpose = purpose;
		td->location = location;
		return td;
	} else {
		return NULL;
	}
}

void free_thing_description(thing_description_t* td) {
	free(td->name);
	free(td->purpose);
	free(td->location);
}

