/*
 * lightBulb.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include <bbreg.h>
#include "coap.h"
#include "json-helper.h"
#include "coap-helper.h"
#include "core-lighting.h"
#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

/* the maximum size of a lighting-config representation that this device can handle */
#define CONF_MAX_SIZE 128
/* defines the interval in which receiving is interrupted to send out observe responses */
#define RESPONSE_TIMER_SEC 1
/* default application log level */
#define LOG_LEVEL LOG_ERR

/* options modifyable by cmdline arguments */
char* bulletins_uri = NULL;
char* config_res_name = NULL;
int port = -1;
char* device_addr = NULL;
char* name = NULL;
char* purpose = NULL;
char* location = NULL;
int test_mode = 0;
int can_exit = 0;
int log_level = 0;
char* base_uri = NULL;
char* config_res_ref = NULL;

thing_description_t* TD;
linkobj_node_t* links;
lighting_config_t* Conf = NULL;
struct coap_resource_t *config_resource = NULL;

/*
 * Converts RGB to ANSI-256 terminal color
 */
unsigned int rgb_to_ansi(unsigned int r, unsigned int g, unsigned int b) {
	if (r == g && g == b) {
		if (r < 8) return 16;
		if (r > 248) return 231;
		return ((((float) r - 8.0f) / 247.0f) * 24) + 232;
	}

	return 16 + (36 * ((float) r / 255.0f * 5))
			  + (6 * ((float) g / 255.0f * 5))
			  + ((float) b / 255.0f * 5);
}

/*
 * Converts percentage-based HSL to ANSI-256 terminal color
 */
unsigned int hsl_to_ansi256(float h, float s, float l) {
		if (s < 0.001f)
			return rgb_to_ansi(l * 255, l * 255, l * 255);
		float v, v1, v2, v3;
		unsigned int rgb[3] = {0, 0, 0};
		if (l < 0.5f)
			v2 = l * (1 + s);
		else
			v2 = l + s - l * s;
		v1 = 2 * l - v2;
		for (int i = 0; i < 3; i++) {
			v3 = h + 1 / 3.0f * - (i - 1);
			if (v3 < 0) v3++;
			if (v3 > 1) v3--;
			if (6 * v3 < 1) {
				v = v1 + (v2 - v1) * 6 * v3;
			} else if (2 * v3 < 1) {
				v = v2;
			} else if (3 * v3 < 2) {
				v = v1 + (v2 - v1) * (2.0f / 3.0f - v3) * 6;
			} else {
				v = v1;
			}
			rgb[i] = v * 255.0f;
		}
		return rgb_to_ansi(rgb[0], rgb[1], rgb[2]);
}

/*
 * Function representing the lightBulb physically changing state.
 * Hardware specific implementations would go here.
 */
static void change_state(int on, double brightness, char* colorMode, int hue, double saturation) {
	printf("\033[2J\033[1;1H");
	if (on > 0) {
		if(strcmp(colorMode,"hs") == 0) {
			/* clamp values that are out of spec */
			if(hue < 0) hue = 0;
			if(hue >= 360) hue = 360;
			if(brightness < 0.0f) brightness = 0.0f;
			if(brightness > 1.0f) brightness = 1.0f;
			if(saturation < 0.0f) saturation = 0.0f;
			if(saturation > 1.0f) saturation = 1.0f;
			printf("\033[38;5;%dm",hsl_to_ansi256((float) hue / 360.0f,saturation,brightness));
		} else {
			printf("unsupported color format: %s", colorMode);
			printf("\033[38;5;%dm", 255);
		}
		if(brightness >= 0.5f) {
			printf("        \n \
 '.  _  .'  \n \
-=  (~)  =- \n \
 .'  #  '.  \n \
			\n");
		} else {
			printf("        \n \
  .  _  .   \n \
 -  (~)  -  \n \
  .  #  .   \n \
			\n");
		}
		printf("\033[38;5;%dm",255);
	} else {
			printf("        \n \
     _      \n \
    (~)     \n \
     #      \n \
    		\n");
	}

}

static void observe_response_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface,
		const coap_address_t *remote, coap_pdu_t *sent, coap_pdu_t *received, const coap_tid_t id) {
	unsigned char* data;
	size_t data_len;

	if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {
		if (coap_get_data(received, &data_len, &data)) {
			resobj_t ro;
			json_object* root = string_to_json(data, data_len);
			if(parse_response(root, &ro, APPLICATION_LIGHTING_JSON)) {
				content_t* content = ro.content;
				lighting_t* state = content->lighting;

				debug("on: %d, brightness: %f, colorMode: %s\n",state->on, state->brightness, state->colorMode);
				change_state(state->on, state->brightness, state->colorMode, state->hue, state->saturation);
				json_object_put(root);
				free_resourceobject(&ro);

				if(test_mode) can_exit = 1;
			} else {
				info("failed parsing observe response!\n");
				if(test_mode) exit(1);
			}
		} else {
			info("got empty response from LRC!\n");
			if(test_mode) exit(1);
		}
	} else {
		info("failed request to LRC with code %d!\n",received->hdr->code);
		if(test_mode) exit(1);
	}
}

static void observe_resource(coap_context_t* context, const char* server_uri_string) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;

	/* Set destination address */
	coap_split_uri((const unsigned char*) server_uri_string, strlen(server_uri_string), &server_uri);
	cl_set_addr(&server_uri, &dst_addr);

	/* Prepare the request */
	request = coap_pdu_init(COAP_MESSAGE_CON, COAP_REQUEST_GET, coap_new_message_id(context), COAP_MAX_PDU_SIZE);

	coap_add_option(request, COAP_OPTION_SUBSCRIPTION, 0, NULL);
	coap_add_option(request, COAP_OPTION_URI_PATH, server_uri.path.length, server_uri.path.s);

	/* Set the handler and send the request */
	coap_register_response_handler(context, observe_response_handler);
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);
}

static void cancel_observe(coap_context_t* context, const char* server_uri_string) {
	coap_pdu_t* request;
	coap_uri_t server_uri;
	coap_address_t dst_addr;
	//unsigned char buffer[3]; //max size of options

	coap_split_uri((const unsigned char*) server_uri_string, strlen(server_uri_string), &server_uri);
	cl_set_addr(&server_uri, &dst_addr);

	/* Prepare the request */
	request = coap_pdu_init(COAP_MESSAGE_RST, COAP_REQUEST_GET, coap_new_message_id(context), COAP_MAX_PDU_SIZE);

	/* Set the handler and send the rst */
	coap_send_confirmed(context, context->endpoint, &dst_addr, request);
}

static void config_get_request_handler(coap_context_t *context, struct coap_resource_t *resource,
		const coap_endpoint_t *local_interface, coap_address_t *peer, coap_pdu_t *request, str *token,
		coap_pdu_t *response) {
	unsigned char buffer[3]; //max size of options

	static formobj_t Form = {
		.href = "",
		.method = "PUT",
		.accept = "application/lighting-config+json"
	};
	static formobj_node_t forms = {.idf = "update", .type = json_type_object, .form = &Form};
	content_t content = {.type = APPLICATION_LIGHTING_CONFIG_JSON, .config=Conf};
	resobj_t ro = {.forms = &forms, .content = &content};

	unsigned char* res = NULL;
	size_t len = create_response(&ro, &res);
	debug("Sending response (%d byte): %s\n", len, res);

	response->hdr->code = COAP_RESPONSE_CODE(205);
	if (coap_find_observer(resource, peer, token)) {
		coap_add_option(response, COAP_OPTION_OBSERVE, coap_encode_var_bytes(buffer, context->observe), buffer);
	}
	coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buffer, APPLICATION_LIGHTING_CONFIG_JSON),
			buffer);

	coap_add_data(response, len, res);
	free(res);
}

/**
 * String resource PUT handler
 */
static void config_put_handler(coap_context_t *ctx, struct coap_resource_t *resource,
		const coap_endpoint_t *local_interface, coap_address_t *peer, coap_pdu_t *request, str *token,
		coap_pdu_t *response) {
	size_t size;
	unsigned char *data;

	/* coap_get_data() sets size to 0 on error */
	if( !coap_get_data(request, &size, &data) ) {
		debug("get data failed \n");
		/* 4.00 client error: invalid request */
		response->hdr->code = COAP_RESPONSE_CODE(400);
	}
	if (size == 0) {
		debug("missing or invalid payload \n");
		/* 4.00 client error: empty request not valid */
		response->hdr->code = COAP_RESPONSE_CODE(400);
	} else if (size >= CONF_MAX_SIZE) {
		debug("recived config too big: %d \n", size);
		/* 4.13 request entity too large */
		response->hdr->code = COAP_RESPONSE_CODE(413);
	} else if( !cl_media_type_equals(&request, APPLICATION_LIGHTING_CONFIG_JSON)){
		/* 4.15 unsupported content-format */
		response->hdr->code = COAP_RESPONSE_CODE(415);
	} else {
		char* old_src = malloc(strlen(Conf->href)+1);
		strcpy(old_src, Conf->href);

		resobj_t ro;
		json_object* root = string_to_json(data, size);
		if(parse_response(root, &ro, APPLICATION_LIGHTING_CONFIG_JSON)) {
			content_t* content = ro.content;
			lighting_config_t* config = content->config;
			debug("got new config: %s (old src: %s) \n", config->href, old_src);
			free(Conf->href);
			free(Conf);
			Conf = malloc(sizeof(lighting_config_t));
			Conf->href = malloc(strlen(config->href)+1);
			strcpy(Conf->href, config->href);
			if(strcmp(old_src, "") != 0) {
				debug("canceling old subscription to %s \n", old_src);
				cancel_observe(ctx, old_src);
			}
			debug("setting up new subscription to %s \n", Conf->href);
			observe_resource(ctx, Conf->href);

			response->hdr->code = COAP_RESPONSE_CODE(204);
			resource->dirty = 1;
		}
		json_object_put(root);
		free_resourceobject(&ro);
		free(old_src);
	}
}

static void init_resource(coap_context_t *context, char* res_name) {
	coap_resource_t* resource;
	resource = coap_resource_init((const unsigned char*) res_name, strlen(res_name), COAP_RESOURCE_FLAGS_NOTIFY_CON);
	resource->observable = 1;
	coap_register_handler(resource, COAP_REQUEST_GET, config_get_request_handler);
	coap_register_handler(resource, COAP_REQUEST_PUT, config_put_handler);
	coap_add_resource(context, resource);
	config_resource = resource;

	//Post TD to BulletinBoard
	bulletinboard_register(context, bulletins_uri, base_uri, links, TD);
}

void handle_cmdline(int argc, char* argv[]) {
	int option = -1;

	while ((option = getopt(argc, argv, "hvTA:P:B:R:n:p:l")) != -1) {
		switch (option) {
		case 'h':
			printf("Usage: lightBulb [-h] [-v] [-T] [-A addr] [-P port] [-B bb_uri] \n"
			"\t [-R res_name] [-n name] [-p purpose] [-l location] \n\n"
			"Arguments: \n"
			"\t-h \t\t displays this help message \n"
			"\t-v \t\t enables verbose logging \n"
			"\t-T \t\t enables a special test mode used in integration tests \n"
			"\t-A addr \t sets the _base address of this device \n"
			"\t\t\t this can be a hostname or IP address \n"
			"\t-P port \t sets the port the internal CoAP server is bound to \n"
			"\t-B bb_uri \t changes the preconfigured bulletin board URI \n"
			"\t-R res_name \t sets the resource name for the about link relation\n"
			"\t-n name \t sets the name string of the thing description\n"
			"\t-p purpose \t sets the purpose string of the thing description\n"
			"\t-l location \t sets the location string of the thing description\n\n"
			"Example (defaults): \n"
			"\t./lightBulb -P 5683 -B \"coap://127.0.0.1:5555\" -R \"state\" -n \"Light 1\"\n");
			exit(0);
		case 'v':
			log_level = LOG_DEBUG;
			break;
		case 'T':
			test_mode = 1;
			break;
		case 'A':
			device_addr = optarg;
			break;
		case 'P':
			port = strtol(optarg, NULL, 10);
			break;
		case 'B':
			bulletins_uri = optarg;
			break;
		case 'R':
			config_res_name = optarg;
			break;
		case 'n':
			name = optarg;
			break;
		case 'p':
			purpose = optarg;
			break;
		case 'l':
			location = optarg;
			break;
		case '?':
			if (optopt == 'A' || optopt == 'P' || optopt == 'B' || optopt == 'R' || optopt == 'n' || optopt == 'p' || optopt == 'l')
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
			else if (isprint(optopt))
				fprintf(stderr, "Unknown option -%c\n", optopt);
			else
				fprintf(stderr, "Unknown option character \\x%x \n", optopt);
			exit(1);
		default:
			exit(1);
		}
	}
	/* defaults */
	if(!log_level) log_level = LOG_LEVEL;
	if(!bulletins_uri) bulletins_uri = "coap://127.0.0.1:5555";
	if(!config_res_name) config_res_name = "config";
	if(port < 0) port = 5683;
	if(!device_addr) device_addr = "127.0.0.1";
	if(!name) name = "Light 1";
	if(!purpose) purpose = "Lights the table";
	if(!location) location = "living room";
	base_uri = malloc((18 * sizeof(char)) + strlen(device_addr));
	sprintf(base_uri, "coap://%s:%d", device_addr, port);
	size_t res_name_size = strlen(config_res_name);
	config_res_ref = malloc(res_name_size + sizeof(char) + 1);
	strcpy(config_res_ref, "/");
	strcat(config_res_ref, config_res_name);
	linkobj_t* conf_link = malloc(sizeof(linkobj_t));
	*conf_link = (linkobj_t) {config_res_ref, "application/lighting-config+json"};
	links = malloc(sizeof(linkobj_node_t));
	*links = (linkobj_node_t) { .idf = "config", .type=json_type_object, .link = conf_link, .next = NULL };
	TD = malloc(sizeof(thing_description_t));
	*TD = (thing_description_t) {.name = name, .purpose = purpose, .location = location};
}

int main(int argc, char* argv[]) {
	/* initial state and defaults */
	Conf = malloc(sizeof(lighting_config_t));
	Conf->href = strdup("\0");

	handle_cmdline(argc, argv);
	change_state(0, 0, "HSL", 0, 0);

	/* static configuration */
	coap_set_log_level(log_level);
	info("core-lighting %s on port %d\n", TD->name, port);

	/* dynamic CoAP variables */
	coap_context_t* context;
	coap_address_t serv_addr;
	fd_set readfds;
	struct timeval recieve_timer;
	recieve_timer.tv_sec = RESPONSE_TIMER_SEC;
	recieve_timer.tv_usec = 0;

	/* prepare the CoAP server socket */
	coap_address_init(&serv_addr);
	serv_addr.addr.sin.sin_family = AF_INET;
	serv_addr.addr.sin.sin_addr.s_addr = INADDR_ANY;
	serv_addr.addr.sin.sin_port = htons(port);

	/* initialize CoAP stack */
	context = coap_new_context(&serv_addr);
	if (!context)
		exit(EXIT_FAILURE);

	/* initialize resources */
	init_resource(context, config_res_name);

	/*Listen for incoming connections*/
	while (!(can_exit && coap_can_exit(context))) {
		FD_ZERO(&readfds);
		FD_SET(context->sockfd, &readfds);
		int result = select( FD_SETSIZE, &readfds, 0, 0, &recieve_timer);
		if (result < 0) {
			/* socket error */
			debug("socket err\n");
			exit(EXIT_FAILURE);
		} else if (result > 0) {
			/* socket read*/
			if (FD_ISSET(context->sockfd, &readfds)) {
				coap_read(context);
			}
		} else {
			extern int bulletinboard_register_success;
			if(!bulletinboard_register_success) {
				info("failed registering at bulletin board!\n");
				exit(1);
			}
			recieve_timer.tv_sec = RESPONSE_TIMER_SEC;
			recieve_timer.tv_usec = 0;
		}
		coap_check_notify(context);
	}
	if(test_mode) sleep(1);
	coap_free_context(context);
	free(Conf->href);
	free(Conf);
	free(links->link);
	free(links);
	free(TD);
	free(config_res_ref);
	free(base_uri);
	return 0;
}
