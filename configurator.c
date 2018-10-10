/*
 * configurator.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "coap.h"
#include "json.h"
#include "coap-helper.h"
#include "core-lighting.h"

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#define REQUEST_WAIT_SECONDS 3

/* dynamic options, can be overwritten via cli */
char* bulletins_uri = NULL;
char* device_a = NULL; /* name of auto conf device 1 (LRC) */
char* device_b = NULL; /* name of auto conf device 2 (lightBulb) */
lighting_config_t* set_config = NULL;
char* light_config_addr = NULL;

int can_exit = 0;
int auto_run = 0; /* 1 if ran in auto conf mode */

static void lighting_config_put_response_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface,
		const coap_address_t *remote, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id) {

	if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
		info("Light bulb was successfully configured!\n");
	} else if (received->hdr->code == COAP_RESPONSE_CODE(413)) {
		info("Error: Lighting config is too large!\n");
		exit(1);
	} else {
		info("Error: Failed to change lighting config!\n");
		exit(1);
	}
	free(set_config->href);
	free(set_config);
	can_exit = 1;
}

static void send_lighting_config_put_request(coap_context_t* context, const char* server_uri_string,
		lighting_config_t* new_config) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;
	unsigned char buffer[6]; //max size of options

	/* Set destination address */
	if(coap_split_uri((const unsigned char*) server_uri_string, strlen(server_uri_string), &server_uri) < 0) {
		debug("Error: failed splitting uri %s\n", server_uri_string);
		return;
	}
	if(!cl_set_addr(&server_uri, &dst_addr)) {
		debug("Error: failed setting uri to %s\n", server_uri_string);
		return;
	}

	/* Create request data */
	unsigned char* data;
	content_t content = {.type = APPLICATION_LIGHTING_CONFIG_JSON, .config=new_config};
	resobj_t ro = {.content = &content};

	size_t len = create_response(&ro, &data);
	debug("Sending request (%d byte): %s\n", len, data);


	/* Prepare PDU and add options and data */
	unsigned int media_type_len = coap_encode_var_bytes(buffer, APPLICATION_LIGHTING_CONFIG_JSON);
	request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_PUT, coap_new_message_id(context), COAP_MAX_PDU_SIZE);
	coap_add_option(request, COAP_OPTION_URI_PATH, server_uri.path.length, server_uri.path.s);
	coap_add_option(request, COAP_OPTION_CONTENT_TYPE, media_type_len, buffer);
	coap_add_data(request, len, data);
	free(data);

	/* Set the response handler and send the request */
	coap_register_response_handler(context, lighting_config_put_response_handler);
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);
}

static void lighting_config_get_response_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface,
		const coap_address_t *remote, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id) {
	unsigned char* data;
	size_t data_len;

	if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
		if (coap_get_data(received, &data_len, &data)) {
			resobj_t ro;
			json_object* root = string_to_json(data, data_len);
			if (parse_response(root, &ro, APPLICATION_LIGHTING_CONFIG_JSON)) {
				if(!ro.forms) {
					info("did not receive form from light bulb!\n");
					exit(1);
				}
				formobj_node_t* fol = ro.forms;
				if(!(fol->type == json_type_object) || !fol->form) {
					info("received unexpected forms from light bulb!\n");
					exit(1);
				}
				formobj_t* form = fol->form;
				if(!(strcmp(fol->idf, "update") == 0)) {
					info("light bulb did not provide update form object!\n");
					exit(1);
				}
				char* light_form_target;
				if(!ro.base || !form->href) {
					if(form->href && (strcmp(form->href, "") == 0)) {
						light_form_target = strdup(light_config_addr);
					} else {
						info("error: received incomplete uri's from light bulb - cannot build form request\n");
						exit(1);
					}
				} else {
					light_form_target = malloc(strlen(ro.base) + strlen(form->href) + 1);
					sprintf(light_form_target, "%s%s", ro.base, form->href);
				}
				json_object_put(root);
				free_resourceobject(&ro);
				send_lighting_config_put_request(ctx, light_form_target, set_config);
				free(light_form_target);
				free(light_config_addr);
			}
		}
	}
}

static void lighting_config_get_request(coap_context_t* context, char* light_bulb_uri) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;

	/* Set destination address */
	coap_split_uri((const unsigned char*) light_bulb_uri, strlen(light_bulb_uri), &server_uri);
	cl_set_addr(&server_uri, &dst_addr);

	/* Prepare the request */
	request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET, coap_new_message_id(context), COAP_MAX_PDU_SIZE);
	coap_add_option(request, COAP_OPTION_URI_PATH, server_uri.path.length, server_uri.path.s);

	/* Set the handler and send the request */
	coap_register_response_handler(context, lighting_config_get_response_handler);
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);
}

/*
 * Check stdin for a number
 */
static int get_user_input_number() {
	char input[42];
	fgets(input, sizeof(input), stdin);
	size_t nl = strlen(input) - 1;
	if (input[nl] == '\n') {
		input[nl] = '\0';
	}
	for (int i = 0; i < nl; i++) {
		if (!isdigit(input[i])) {
			return -1;
		}
	}
	int ret = strtol(input, NULL, 10);
	return ret;
}

int is_lrc(linkobj_node_t* links) {
	linkobj_node_t* cur = links;
	while(cur) {
		if (cur->link && strcmp(cur->link->type, "application/lighting+json") == 0)
				return 1;
		cur = cur->next;
	}
	return 0;
}

int is_light(linkobj_node_t* links) {
	linkobj_node_t* cur = links;
	while(cur) {
		if (cur->link && strcmp(cur->link->type, "application/lighting-config+json") == 0)
				return 1;
		cur = cur->next;
	}
	return 0;
}

static void bulletins_get_response_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface,
		const coap_address_t *remote, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id) {
	unsigned char* data;
	size_t data_len;

	if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
		if (coap_get_data(received, &data_len, &data)) {
			resobj_t ro;
			json_object* root = string_to_json(data, data_len);
			if (parse_response(root, &ro, APPLICATION_BULLETIN_BOARD_JSON)) {
				if(!ro.embedded || !ro.embedded->embedded) {
					return;
				}
				embdobj_node_t* eol = ro.embedded;
				if(!eol || !eol->embedded) {
					return;
				}
				if(!(strcmp(eol->idf, "item") == 0) || !(eol->type == json_type_array)) {
					return;
				}
				resobj_array_t* roa = eol->embedded_arr;
				size_t size = roa->size;
				json_object** bulletin_json = roa->roptr;
				resobj_t** bulletins = (resobj_t**) malloc(size * sizeof(resobj_t*));
				for(int i = 0; i < size; i++) {
					bulletins[i] = (resobj_t*) malloc(sizeof(resobj_t));
					if(!json_decode_resourceobject(bulletin_json[i], bulletins[i], APPLICATION_THINGDESCRIPTION_JSON)) {
						info("error decoding resource object\n");
					}
				}
				if(size <= 0) {
					return;
				}

				debug("got %d things from bulletin board\n", size);

				int a = -1;
				int b = -1;

				if (auto_run) {
					/* auto conf mode */
					for (int i = 0; i < size; i++) {
						if(!bulletins[i] || !bulletins[i]->content ||
								!(bulletins[i]->content->type == APPLICATION_THINGDESCRIPTION_JSON) ||
								!bulletins[i]->base || !bulletins[i]->links) {
							continue;
						}
						thing_description_t* td = bulletins[i]->content->td;
						if (strcmp(td->name, device_a) == 0) {
							a = i;
						} else if (strcmp(td->name, device_b) == 0) {
							b = i;
						}
					}
					if (a == -1) {
						info("Error: auto conf could not find device named %s\n", device_a);
						exit(1);
					}
					if (b == -1) {
						info("Error: auto conf could not find device named %s\n", device_b);
						exit(1);
					}
				} else {
					/* interactive mode */
					info("  --- AVAILABLE DEVICES ---\n");

					for (int i = 0; i < size; i++) {
						if(!bulletins[i] || !bulletins[i]->content ||
								!(bulletins[i]->content->type == APPLICATION_THINGDESCRIPTION_JSON) ||
								!bulletins[i]->base || !bulletins[i]->links) {
							continue;
						}
						thing_description_t* td = bulletins[i]->content->td;
						if (is_light(bulletins[i]->links)) {
							info("%d) Light at %s \n", i, bulletins[i]->base);
						} else if (is_lrc(bulletins[i]->links)) {
							info("%d) LRC at %s \n", i, bulletins[i]->base);
						} else {
							info("%d) Other device at %s \n", i, bulletins[i]->base);
						}
						info("\t name: %s \n", td->name);
						info("\t purpose: %s \n", td->purpose);
						info("\t location: %s \n", td->location);
					}

					/* Prompt user for input of light */
					int valid = 0;
					while (!valid) {
						info("\n Choose a LRC (enter number):\n");
						a = get_user_input_number();
						if(a >= size) {
							info("invalid input - number not in range 0-%d\n", size-1);
						} else if(a < 0) {
							info("invalid input - only numbers >0 are allowed\n");
						} else {
							if (is_lrc(bulletins[a]->links)) {
								valid = 1;
							} else {
								info("Error: %d is not a LRC!\n", a);
							}
						}
					}

					/* Prompt user for input of light */
					valid = 0;
					while (!valid) {
						info("Choose a light (enter number):\n");
						b = get_user_input_number();
						if(b >= size) {
							info("invalid input - number not in range 0-%d\n", size-1);
						} else if(b < 0) {
							info("invalid input - only numbers >0 are allowed\n");
						} else {
							if (is_light(bulletins[b]->links)) {
								valid = 1;
							} else {
								info("Error: %d is not a light!\n", b);
							}
						}

					}
				}
				if (a >= 0 && b >= 0) {
					info("Connecting %s to %s\n", bulletins[a]->content->td->name, bulletins[b]->content->td->name);
					char* lrc_state_addr = NULL;
					linkobj_node_t* cur = bulletins[a]->links;
					while(cur) {
						if (cur->link && strcmp(cur->link->type, "application/lighting+json") == 0) {
							lrc_state_addr = malloc(strlen(bulletins[a]->base) + strlen(cur->link->href) + 1);
							sprintf(lrc_state_addr, "%s%s", bulletins[a]->base, cur->link->href);
							break;
						}
						cur = cur->next;
					}

					light_config_addr = NULL;
					cur = bulletins[b]->links;
					while(cur) {
						if (cur->link && strcmp(cur->link->type, "application/lighting-config+json") == 0) {
							light_config_addr = malloc(strlen(bulletins[b]->base) + strlen(cur->link->href) + 1);
							sprintf(light_config_addr, "%s%s", bulletins[b]->base, cur->link->href);
							break;
						}
						cur = cur->next;
					}
					set_config = malloc(sizeof(lighting_config_t));
					*set_config = (lighting_config_t) { lrc_state_addr };

					lighting_config_get_request(ctx, light_config_addr);
				}
				for(int i = 0; i < size; i++) {
					free_resourceobject(bulletins[i]);
					free(bulletins[i]);
				}
				free(bulletins);
				free_resourceobject(&ro);
			}
			json_object_put(root);
		}
	}
}

static void bulletins_get_request(coap_context_t* context, const char* server_uri_string) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;

	/* Set destination address */
	coap_split_uri((const unsigned char*) server_uri_string, strlen(server_uri_string), &server_uri);
	cl_set_addr(&server_uri, &dst_addr);

	/* Prepare the request */
	request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET, coap_new_message_id(context), COAP_MAX_PDU_SIZE);
	coap_add_option(request, COAP_OPTION_URI_PATH, server_uri.path.length, server_uri.path.s);

	/* Set the handler and send the request */
	coap_register_response_handler(context, bulletins_get_response_handler);
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);
}

void handle_cmdline(int argc, char* argv[]) {
	int option = -1;

	while ((option = getopt(argc, argv, "hB:")) != -1) {
		switch (option) {
		case 'h':
			printf(	"Usage: configurator [-h] [-B bb_uri] [lrc-name:light-name]\n"
					"\t if used, lrc-name and light-name must exactly match the name string\n"
					"\t from the thing description of the devices that should be configured\n\n"
					"Arguments: \n"
					"\t-h \t\t displays this help message \n"
					"\t-B bb_uri \t changes the preconfigured bulletin board URI \n\n"
					"Example (with auto conf): \n"
					"\t./configurator -B \"coap://127.0.0.1:5555\" \"LRC 1:Light 1\"\n");
			exit(0);
		case 'B':
			bulletins_uri = strdup(optarg);
			break;
		case '?':
			if (optopt == 'B')
				fprintf(stderr, "Option -B requires a URI as argument\n");
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option -%c\n", optopt);
			else
				fprintf(stderr, "Unknown option character \\x%x \n", optopt);
			exit(1);
		default:
			exit(1);
		}
	}

	for (int index = optind; index < argc; index++) {
		if (strchr(argv[index], ':')) {
			device_a = strsep(&argv[index], ":");
			device_b = strsep(&argv[index], ":");
			info("automatically connecting %s to %s", device_a, device_b);
			auto_run = 1;
		} else {
			fprintf(stderr, "To automatically configure devices they must be supplied as \"lrc-name:light-name\"\n");
			exit(1);
		}
	}

	if(!bulletins_uri) bulletins_uri = strdup("coap://127.0.0.1:5555");
}

int main(int argc, char* argv[]) {
	handle_cmdline(argc, argv);
	coap_set_log_level(LOG_INFO);
	info("core-lighting configurator\n");

	coap_context_t* context;
	coap_address_t src_addr;
	fd_set readfds;
	coap_tick_t max_wait;
	coap_queue_t *nextpdu;
	coap_tick_t now;
	struct timeval receive_timer;

	cl_server_addr(&src_addr);
	context = coap_new_context(&src_addr);

	bulletins_get_request(context, bulletins_uri);

	coap_ticks(&max_wait);
	max_wait += REQUEST_WAIT_SECONDS * COAP_TICKS_PER_SECOND;

	nextpdu = coap_peek_next(context);

	coap_ticks(&now);
	while (nextpdu && nextpdu->t <= now - context->sendqueue_basetime) {
		coap_retransmit(context, coap_pop_next(context));
		nextpdu = coap_peek_next(context);
	}

	if (nextpdu && nextpdu->t < max_wait - now) {
		receive_timer.tv_usec = ((nextpdu->t) % COAP_TICKS_PER_SECOND) * 1000000 / COAP_TICKS_PER_SECOND;
		receive_timer.tv_sec = (nextpdu->t) / COAP_TICKS_PER_SECOND;
	} else {
		receive_timer.tv_usec = ((max_wait - now) % COAP_TICKS_PER_SECOND) * 1000000 / COAP_TICKS_PER_SECOND;
		receive_timer.tv_sec = (max_wait - now) / COAP_TICKS_PER_SECOND;
	}

	while (!(can_exit && coap_can_exit(context))) {
		FD_ZERO(&readfds);
		FD_SET(context->sockfd, &readfds);

		int result = select( FD_SETSIZE, &readfds, 0, 0, &receive_timer);
		if (result < 0) {
			/* socket error */
			exit(EXIT_FAILURE);
		} else if (result > 0) {
			/* socket read*/
			if (FD_ISSET(context->sockfd, &readfds)) {
				coap_read(context);
			}
		} else {
			/* socket timeout */
			coap_ticks(&now);
			if (max_wait <= now) {
				info("Error: request timed out!\n");
				exit(1);
			}
			receive_timer.tv_sec = 0;
			receive_timer.tv_usec = 0;
		}
	}
	sleep(1);
	coap_free_context(context);
	free(bulletins_uri);
	return 0;
}
