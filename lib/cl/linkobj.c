/*
 *	linkobj.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */

#include "linkobj.h"
#include "json.h"
#include "json-helper.h"

void json_append_link_objects(linkobj_node_t* lnode, json_object* root) {
	while (lnode) {
		json_object* json_array;
		json_object* item;
		switch(lnode->type) {
			case json_type_object:
				item = json_object_new_object();
				JSON_PUT_STR(item, "href", lnode->link->href);
				JSON_PUT_STR(item, "type", lnode->link->type);
				json_object_object_add(root, lnode->idf, item);
				break;
			case json_type_array:
				json_array = json_object_new_array();
				linkobj_array_t* lo_arr = lnode->link_arr;
				for(int i = 0; i < lo_arr->size; i++) {
					item = json_object_new_object();
					JSON_PUT_STR(item, "href", lo_arr->lptr[i]->href);
					JSON_PUT_STR(item, "type", lo_arr->lptr[i]->type);
					json_object_array_put_idx(json_array, i, item);
				}
				json_object_object_add(root, lnode->idf, json_array);
				break;
			default:
				break;

		}
	lnode = lnode->next;
	}
}

int json_decode_link_objects(json_object* root, linkobj_node_t** lnode) {
	json_object* _links = NULL;
	JSON_GET_OBJ_OR_ERR(root, "_links", _links);
	linkobj_node_t* head = NULL;
	linkobj_node_t* prev = NULL;
	json_object_object_foreach(_links, k, v) {
		linkobj_node_t* cur = malloc(sizeof(linkobj_node_t));
		cur->next = NULL;
		cur->idf = k;

		if (json_object_get_type(v) == json_type_object) {
			cur->link = malloc(sizeof(linkobj_t));
			cur->type = json_type_object;
			json_object* obj = (struct json_object*) v;
			JSON_GET_STRING_OR_ERR(obj, "href", &cur->link->href);
			JSON_GET_STRING_OR_ERR(obj, "type", &cur->link->type);
		}  else if(json_object_get_type(v) == json_type_array) {
			cur->type = json_type_array;
			json_object* item_array = (json_object*) v;
			int item_array_size = json_object_array_length(v);
			cur->link_arr = malloc(sizeof(linkobj_array_t));
			cur->link_arr->lptr = malloc(item_array_size * sizeof(linkobj_t*));
			cur->link_arr->size = item_array_size;

			json_object* item = NULL;
			for(int i = 0; i < item_array_size; i++) {
				item = json_object_array_get_idx(item_array, i);
				JSON_GET_STRING_OR_ERR(item, "href", &cur->link_arr->lptr[i]->href);
				JSON_GET_STRING_OR_ERR(item, "type", &cur->link_arr->lptr[i]->type);
			}
		}

		if (!head)
			head = cur;
		if (prev)
			prev->next = cur;
		prev = cur;
	}
	*lnode = head;
	return 1;
}
