/*
 * formobj.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */

#include "formobj.h"
#include "json-helper.h"

void json_append_form_objects(formobj_node_t* fnode, json_object* root) {
	while(fnode) {
		json_object* json_array;
		json_object* item;
		switch(fnode->type) {
			case json_type_object:
				item = json_object_new_object();
				JSON_PUT_STR(item, "href", fnode->form->href);
					JSON_PUT_STR(item, "method", fnode->form->method);
					JSON_PUT_STR(item, "accept", fnode->form->accept);
				json_object_object_add(root, fnode->idf, item);
				break;
			case json_type_array:
				json_array = json_object_new_array();
				formobj_array_t* fo_arr = fnode->form_arr;
				for(int i = 0; i < fo_arr->size; i++) {
					item = json_object_new_object();
					JSON_PUT_STR(item, "href", fo_arr->foptr[i]->href);
					JSON_PUT_STR(item, "method", fo_arr->foptr[i]->method);
					JSON_PUT_STR(item, "accept", fo_arr->foptr[i]->accept);
					json_object_array_put_idx(json_array, i, item);
				}
				json_object_object_add(root, fnode->idf, json_array);
				break;
			default:
				break;

		}
		fnode = fnode->next;
	}
}

int json_decode_form_objects(json_object* root, formobj_node_t** fnode) {
	json_object* _forms = NULL;
	JSON_GET_OBJ_OR_ERR(root, "_forms", _forms);
	formobj_node_t* head = NULL;
	formobj_node_t* prev = NULL;
	json_object_object_foreach(_forms, k, v) {
		formobj_node_t* cur = malloc(sizeof(formobj_node_t));
		cur->next = NULL;
		cur->idf = k;

		if (json_object_get_type(v) == json_type_object) {
			cur->form = malloc(sizeof(formobj_t));
			cur->type = json_type_object;
			json_object* obj = (struct json_object*) v;
			JSON_GET_STRING_OR_ERR(obj, "href", &cur->form->href);
			JSON_GET_STRING_OR_ERR(obj, "method", &cur->form->method);
			JSON_GET_STRING_OR_ERR(obj, "accept", &cur->form->accept);
		}  else if(json_object_get_type(v) == json_type_array) {
			cur->type = json_type_array;
			json_object* item_array = (json_object*) v;
			int item_array_size = json_object_array_length(v);
			cur->form_arr = malloc(sizeof(formobj_array_t));
			cur->form_arr->foptr = malloc(item_array_size * sizeof(formobj_t*));
			cur->form_arr->size = item_array_size;

			json_object* item = NULL;
			for(int i = 0; i < item_array_size; i++) {
				item = json_object_array_get_idx(item_array, i);
				JSON_GET_STRING_OR_ERR(item, "href", &cur->form_arr->foptr[i]->href);
				JSON_GET_STRING_OR_ERR(item, "method", &cur->form_arr->foptr[i]->method);
				JSON_GET_STRING_OR_ERR(item, "accept", &cur->form_arr->foptr[i]->accept);
			}
		}

		if (!head)
			head = cur;
		if (prev)
			prev->next = cur;
		prev = cur;
	}
	*fnode = head;
	return 1;
}
