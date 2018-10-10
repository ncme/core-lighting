/*
 * bbreg.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "bbreg.h"
#include "coap.h"
#include "json.h"
#include "coap-helper.h"
#include "core-lighting.h"

#include <stdio.h>

resobj_t* resobj = NULL;
int bulletinboard_register_success = 0;

void bulletinboard_register_response_handler(struct coap_context_t *ctx,
		const coap_endpoint_t *local_interface,
		const coap_address_t *remote, coap_pdu_t *sent,
		coap_pdu_t *received, const coap_tid_t id) {
	if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
		bulletinboard_register_success = 1;
		debug("Successfully registered at bulletin board!\n");
	} else {
		info("Error: Could not register at bulletin board! Response code %d\n",
				received->hdr->code);
		exit(1);
	}
}

void bulletinboard_register_at(coap_context_t* context,
		const char* server_uri_string, int code, resobj_t* ro) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;
	unsigned char buffer[6]; //max size of options

	/* Set destination address */
	coap_split_uri((const unsigned char*) server_uri_string,
			strlen(server_uri_string), &server_uri);
	cl_set_addr(&server_uri, &dst_addr);

	unsigned char* req = NULL;
	size_t len = create_response(ro, &req);
	free_resourceobject(ro);
	free(ro);
	debug("Sending TD to bulletin board (%d byte): %s\n", len, req);

	/* Prepare the request */
	request = coap_pdu_init(COAP_MESSAGE_CON, code,
			coap_new_message_id(context),
			COAP_MAX_PDU_SIZE);

	coap_add_option(request, COAP_OPTION_URI_PATH,
			server_uri.path.length, server_uri.path.s);
	coap_add_option(request, COAP_OPTION_CONTENT_TYPE,
			coap_encode_var_bytes(buffer,
					APPLICATION_THINGDESCRIPTION_JSON), buffer);

	coap_add_data(request, len, req);

	/* Set the handler and send the request */
	coap_register_response_handler(context,
			bulletinboard_register_response_handler);
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);

	free(req);
}

void bulletinboard_get_response_handler(struct coap_context_t *ctx,
		const coap_endpoint_t *local_interface,
		const coap_address_t *remote, coap_pdu_t *sent,
		coap_pdu_t *received, const coap_tid_t id) {
	unsigned char* data;
	size_t data_len;

	if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
		if (coap_get_data(received, &data_len, &data)) {
			resobj_t ro;
			json_object* root = string_to_json(data, data_len);
			if (parse_response(root, &ro, APPLICATION_BULLETIN_BOARD_JSON)) {
				if(!ro.forms) {
					info("error: bulletin board does not provide a form\n");
					exit(1);
				}
				formobj_node_t* form_node = ro.forms;
				while(form_node) {
					if((strcmp(form_node->idf, "create-item") == 0) || !(form_node->type == json_type_object)) {
						break;
					}
					form_node = form_node->next;
				}

				if(!form_node) {
					info("error: bulletin board does not provide a create-item form\n");
					exit(1);
				}

				formobj_t* form = form_node->form;
				if(!form) {
					info("error: bulletin board does not provide a complete create-item form object\n");
					exit(1);
				}

				if(strcmp(form->accept, "application/thing-description+json") == 0) {
					int code = -1;
					if(strcmp(form->method, "POST") == 0) {
						code = COAP_REQUEST_POST;
					} else if(strcmp(form->method, "PUT") == 0) {
						code = COAP_REQUEST_PUT;
					} else {
						info("error: bulletin board form contains unsupported method: %s\n", form->method);
						exit(1);
					}

					if(ro.base && form->href) {
						char* bb_form_target = malloc(strlen(ro.base) + strlen(form->href) + 1);
						sprintf(bb_form_target, "%s%s", ro.base, form->href);

						json_object_put(root);
						free_resourceobject(&ro);
						bulletinboard_register_at(ctx, bb_form_target, code, resobj);
						free(bb_form_target);
					} else {
						info("error: received incomplete information from bulletin board to build form request\n");
						exit(1);
					}
				}
			} else {
				info("error: could not parse bulletin board representation\n");
				exit(1);
			}
		} else {
			info("error: received invalid or empty response from bulletin board\n");
			exit(1);
		}
	} else {
		info("Error: Could not get representation from bulletin board! Response code %d\n",
				received->hdr->code);
		exit(1);
	}
}

void bulletinboard_get_request(coap_context_t* context, const char* server_uri_string) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;

	/* Set destination address */
	coap_split_uri((const unsigned char*) server_uri_string,
			strlen(server_uri_string), &server_uri);
	cl_set_addr(&server_uri, &dst_addr);

	/* Prepare the request */
	request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET,
			coap_new_message_id(context),
			COAP_MAX_PDU_SIZE);

	coap_add_option(request, COAP_OPTION_URI_PATH,
			server_uri.path.length, server_uri.path.s);

	/* Set the handler and send the request */
	coap_register_response_handler(context,
			bulletinboard_get_response_handler);
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);
}

void bulletinboard_register(coap_context_t* context,
		const char* server_uri, char* base_uri, linkobj_node_t* links, thing_description_t* td ) {
	/* copy td and links so that they dont get free'd when ro is destroyed */
	thing_description_t* td_cpy = malloc(sizeof(thing_description_t));
	memcpy(td_cpy, td, sizeof(thing_description_t));
	linkobj_node_t* links_cpy = malloc(sizeof(linkobj_node_t));
	memcpy(links_cpy, links, sizeof(linkobj_node_t));
	linkobj_t* link_cpy = malloc(sizeof(linkobj_t));
	memcpy(link_cpy, links->link, sizeof(linkobj_t));
	links_cpy->link = link_cpy;
	content_t* content = malloc(sizeof(content_t));
	*content = (content_t) { .type = APPLICATION_THINGDESCRIPTION_JSON, .td = td_cpy };
	resobj_t* ro = malloc(sizeof(resobj_t));
	*ro = (resobj_t) {.base = base_uri, .links = links_cpy, .content = content};

	resobj = ro;
	bulletinboard_get_request(context, server_uri);
}




