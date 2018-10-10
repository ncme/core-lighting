/*
 * embdobj.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "embdobj.h"
#include "resobj.h"
#include "json.h"
#include "json-helper.h"

void json_append_embedded_objects(embdobj_node_t* enode, json_object* root) {
	while (enode) {
		json_object* json_array;
		switch(enode->type) {
			case json_type_object:
				json_object_get(enode->embedded); /* ensure obj is not removed */
				json_object_object_add(root, enode->idf, enode->embedded);
				break;
			case json_type_array:
				json_array = json_object_new_array();
				resobj_array_t* ro_arr = enode->embedded_arr;
				for(int i = 0; i < ro_arr->size; i++) {
					json_object_get(ro_arr->roptr[i]); /* ensure obj is not removed */
					json_object_array_put_idx(json_array, i, ro_arr->roptr[i]);
				}
				json_object_object_add(root, enode->idf, json_array);
				break;
			default:
				break;

		}
	enode = enode->next;
	}
}

int json_decode_embedded_objects(json_object* root, embdobj_node_t** enode) {
	json_object* _embedded = NULL;
	JSON_GET_OBJ_OR_ERR(root, "_embedded", _embedded);
	embdobj_node_t* head = NULL;
	embdobj_node_t* prev = NULL;
	json_object_object_foreach(_embedded, k, v) {
		embdobj_node_t* cur = malloc(sizeof(embdobj_node_t));
		cur->next = NULL;
		cur->idf = k;

		if (json_object_get_type(v) == json_type_object) {
			cur->type = json_type_object;
			cur->embedded = (struct json_object*) v;
		}  else if(json_object_get_type(v) == json_type_array) {
			cur->type = json_type_array;
			json_object* item_array = (json_object*) v;
			int item_array_size = json_object_array_length(v);
			cur->embedded_arr = malloc(sizeof(resobj_array_t));
			cur->embedded_arr->roptr = malloc(item_array_size * sizeof(json_object*));
			cur->embedded_arr->size = item_array_size;

			json_object* item = NULL;
			for(int i = 0; i < item_array_size; i++) {
				item = json_object_array_get_idx(item_array, i);
				cur->embedded_arr->roptr[i] = item;
			}
		}

		if (!head)
			head = cur;
		if (prev)
			prev->next = cur;
		prev = cur;
	}
	*enode = head;
	return 1;
}

