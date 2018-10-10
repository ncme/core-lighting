/*
 * resobj.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "resobj.h"
#include "linkobj.h"
#include "formobj.h"
#include "embdobj.h"
#include "content.h"
#include "json.h"
#include "json-helper.h"

void json_append_resourceobject(resobj_t* ro, json_object* root) {
	if (ro->base) { /* add base URI */
		JSON_PUT_STR(root, "_base", ro->base);
	}
	if (ro->links) { /* add LinkObjects */
		json_object* __links = json_object_new_object();
		json_object_object_add(root, "_links", __links);
		json_append_link_objects(ro->links, __links);
	}
	if (ro->forms) { /* add FormObjects */
		json_object* __forms = json_object_new_object();
		json_object_object_add(root, "_forms", __forms);
		json_append_form_objects(ro->forms, __forms);

	}
	if (ro->embedded) { /* add EmbeddedObjects */
		json_object* __embedded = json_object_new_object();
		json_object_object_add(root, "_embedded", __embedded);
		json_append_embedded_objects(ro->embedded, __embedded);

	}
	if (ro->content) { /* add Content */
		switch (ro->content->type) {
		case APPLICATION_LIGHTING_JSON:
			json_encode_lighting(ro->content->lighting, root);
			break;
		case APPLICATION_LIGHTING_CONFIG_JSON:
			json_encode_lightingconfig(ro->content->config, root);
			break;
		case APPLICATION_THINGDESCRIPTION_JSON:
			json_encode_thingdescription(ro->content->td, root);
			break;
		default:
			break;
		}
	}
}

int json_decode_resourceobject(json_object* root, resobj_t* ro,
		content_type content_type) {
	char* base = NULL;
	linkobj_node_t* links = NULL;
	formobj_node_t* forms = NULL;
	embdobj_node_t* embedded = NULL;
	content_t* content = NULL;
	int ret = 0;
	json_object_object_foreach(root, key, val) {
		if (strcmp(key, "_base") == 0
				&& json_object_get_type(val) == json_type_string) {
			JSON_GET_STRING_OR_ERR(root, "_base", &base);
		} else if (strcmp(key, "_links") == 0
				&& json_object_get_type(val) == json_type_object) {
			ret = json_decode_link_objects(root, &links);
			if (!ret)
				return ret;
		} else if (strcmp(key, "_forms") == 0
				&& json_object_get_type(val) == json_type_object) {
			ret = json_decode_form_objects(root, &forms);
			if (!ret)
				return ret;
		} else if (strcmp(key, "_embedded") == 0
				&& json_object_get_type(val) == json_type_object) {
			ret = json_decode_embedded_objects(root, &embedded);
			if (!ret)
				return ret;
		}
	}
	if (content_type != CONTENT_TYPE_NONE) { /* decode content */
		lighting_t* lighting = NULL;
		lighting_config_t* config = NULL;
		thing_description_t* td = NULL;
		content = malloc(sizeof(content_t));
		switch (content_type) {
		case APPLICATION_LIGHTING_JSON:
			lighting = malloc(sizeof(lighting_t));
			ret = json_decode_lighting(root, lighting);
			*content = (content_t ) { .type = content_type,
				.lighting = lighting };
			break;
		case APPLICATION_LIGHTING_CONFIG_JSON:
			config = malloc(sizeof(lighting_config_t));
			ret = json_decode_lightingconfig(root, config);
			*content = (content_t ) { .type = content_type, .config =
							config };
			break;
		case APPLICATION_THINGDESCRIPTION_JSON:
			td = malloc(sizeof(thing_description_t));
			ret = json_decode_thingdescription(root, td);
			*content = (content_t ) { .type = content_type, .td = td };
			break;
		default:
			free(content);
			content = NULL;
			break;
		}
		if (!ret)
			return ret;
	}
	*ro = (resobj_t ) { .base = base, .links = links, .forms =
					forms, .embedded = embedded, .content = content };
	return 1;
}

void free_resourceobject(resobj_t* ro) {
	if (!ro)
		return;

	if (ro->links) {
		linkobj_node_t* cur = ro->links;
		linkobj_node_t* last;
		while (cur != NULL) {
			last = cur;
			cur = cur->next;
			if(last->type == json_type_object)
				free(last->link);
			if(last->type == json_type_array) {
				for(int i = 0; i < last->link_arr->size; i++)
					free(last->link_arr->lptr[i]);
				free(last->link_arr->lptr);
				free(last->link_arr);
			}
			free(last);
		}
	}
	if (ro->forms) {
		formobj_node_t* cur = ro->forms;
		formobj_node_t* last;
		while (cur != NULL) {
			last = cur;
			cur = cur->next;
			if(last->type == json_type_object)
				free(last->form);
			if(last->type == json_type_array) {
				for(int i = 0; i < last->form_arr->size; i++)
					free(last->form_arr->foptr[i]);
				free(last->form_arr->foptr);
				free(last->form_arr);
			}
			free(last);
		}
	}
	if (ro->embedded) {
		embdobj_node_t* cur = ro->embedded;
		embdobj_node_t* last;
		while (cur != NULL) {
			last = cur;
			cur = cur->next;
			if (last->embedded) {
				if(last->type == json_type_array) {
					free(last->embedded_arr->roptr);
					free(last->embedded_arr);
				}
			}
			free(last);
		}
	}
	if (ro->content) {
		switch (ro->content->type) {
		case APPLICATION_LIGHTING_JSON:
			free(ro->content->lighting);
			break;
		case APPLICATION_LIGHTING_CONFIG_JSON:
			free(ro->content->config);
			break;
		case APPLICATION_THINGDESCRIPTION_JSON:
			free(ro->content->td);
			break;
		default:
			break;
		}
		free(ro->content);
	}
}
