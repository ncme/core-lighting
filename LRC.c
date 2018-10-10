/*
 * LRC.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include <bbreg.h>
#include "coap.h"
#include "core-lighting.h"

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>

/* the maximum size of a lighting representation that this device can handle */
#define MAX_STATE_SIZE 128
/* defines the interval in which receiving is interrupted to send out observe responses */
#define RESPONSE_TIMER_SEC 1
/* the number of intervals to wait until the bulletin board interaction times out */
#define BULLETINBOARD_RESPONSE_TIMEOUT 3
/* default application log level */
#define LOG_LEVEL LOG_INFO

/* options modifyable by cmdline arguments */
char* bulletins_uri = NULL;
char* state_res_name = NULL;
int port = -1;
char* device_addr = NULL;
char* name = NULL;
char* purpose = NULL;
char* location = NULL;
int test_mode = 0;
int can_exit = 0;
int log_level = 0;
int bulletin_put_success = 0;
char* base_uri;
char* state_res_ref;

thing_description_t* TD;
linkobj_node_t* links;
coap_resource_t *state_resource = NULL; /* the /state resource */
lighting_t* state = NULL; /* the internal representation of the lighting state */

static void state_get_request_handler(coap_context_t *context, struct coap_resource_t *resource, const coap_endpoint_t *local_interface,
		coap_address_t *peer, coap_pdu_t *request, str *token, coap_pdu_t *response) {
	unsigned char buffer[3]; //max size of options

	if (request && request->hdr->type == COAP_MESSAGE_RST) {
		debug("got RST\n");
		if (coap_find_observer(resource, peer, token)) {
			coap_delete_observer(resource, peer, token);
		}
		return;
	}

	content_t content = (content_t) {.type = APPLICATION_LIGHTING_JSON, .lighting=state};
	resobj_t ro = (resobj_t) {.content = &content};

	unsigned char* res = NULL;
    size_t len = create_response(&ro, &res);
	debug("Sending response (%d byte): %s to %s\n", len, res, ro.base);


	response->hdr->code = COAP_RESPONSE_CODE(205);
	if (coap_find_observer(resource, peer, token)) {
	    coap_add_option(response, COAP_OPTION_OBSERVE, coap_encode_var_bytes(buffer, context->observe), buffer);
	}
	coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buffer, APPLICATION_LIGHTING_JSON), buffer);

	coap_add_data(response, len, res);

	free(res);
	if(test_mode) can_exit = 1;
}

static void init_resource(coap_context_t *context, char* res_name) {
	coap_resource_t* resource;
	resource = coap_resource_init((const unsigned char*) res_name, strlen(res_name), COAP_RESOURCE_FLAGS_NOTIFY_CON);
	resource->observable = 1;
	coap_register_handler(resource, COAP_REQUEST_GET, state_get_request_handler);
	coap_add_resource(context, resource);
	state_resource = resource;

	//Post TD to BulletinBoard
	bulletinboard_register(context, bulletins_uri, base_uri, links, TD);
}

/*
 * modify LRC state based on CLI inputs
 */
static void cli_modify_state(char* input) {
	int numRead = read(0,input,20);
	if(numRead <= 0){
		memset(input, '\0', 20);
		return;
	}
	if(strcmp("on\n", input) == 0) {
		info(">Lights on!\n");
		if(state->on != 1) {
			state->on = 1;
			state_resource->dirty = 1;
		}
	} else if(strcmp("off\n", input) == 0) {
		info(">Lights off!\n");
		if(state->on != 0) {
			state->on = 0;
			state_resource->dirty = 1;
		}
	} else if(strstr(input, "brightness")) {
		float brightness = -1.0f;
		if(sscanf(input, "brightness(%f)\n",&brightness) == 1 && brightness >= 0) {
			state->brightness = brightness;
			info(">Setting new brightness: %f\n", brightness);
			state_resource->dirty = 1;
		} else {
			info(">Usage: brightness(FLOAT)\n");
		}
	} else if(strstr(input, "hs")) {
		int hue = -1;
		float sat = -1.0f;
		if(sscanf(input, "hs(%d,%f)\n",&hue,&sat) == 2 && hue >= 0 && sat >= 0) {
			state->colorMode = "hs";
			state->hue = hue;
			state->saturation = sat;
			state->colorTemperature = 0;
			state->x = 0.0;
			state->y = 0.0;
			info(">Setting new color: hs(%d,%f)\n",hue,sat);
			state_resource->dirty = 1;
		} else {
			info(">Usage: hs(INT,FLOAT)\n");
		}
	} else if(strstr(input, "xy")) {
		float x = -1.0f;
		float y = -1.0f;
		if(sscanf(input, "xy(%f,%f)\n",&x,&y) == 2 && x >= 0 && y >= 0) {
			state->colorMode = "xy";
			state->hue = 0;
			state->saturation = 0.0;
			state->colorTemperature = 0;
			state->x = x;
			state->y = y;
			info(">Setting new color:  xy(%f,%f)\n",x,y);
			state_resource->dirty = 1;
		} else {
			info(">Usage: xy(FLOAT,FLOAT)\n");
		}
	} else if(strstr(input, "ct")) {
		int ct = -1;
		if(sscanf(input, "ct(%d)\n",&ct) == 1 && ct >= 0) {
			state->colorMode = "ct";
			state->hue = 0;
			state->saturation = 0.0;
			state->colorTemperature = ct;
			state->x = 0.0;
			state->y = 0.0;
			info(">Setting new color: color temperature %dk\n",ct);
			state_resource->dirty = 1;
		} else {
			info(">Usage: ct(INT)\n");
		}
	} else {
		info(">Unknown command!\n");
	}
	memset(input, '\0', 20);
}

static void handle_cmdline(int argc, char* argv[]) {
	int option = -1;

	while ((option = getopt(argc, argv, "hvTA:P:B:R:n:p:l")) != -1) {
		switch (option) {
		case 'h':
			printf("Usage: LRC [-h] [-v] [-T] [-A addr] [-P port] [-B bb_uri]\n"
			"\t [-R res_name] [-n name] [-p purpose] [-l location] \n\n"
			"Arguments: \n"
			"\t-h \t\t displays this help message \n"
			"\t-v \t\t enables verbose logging \n"
			"\t-T \t\t enables a special test mode used in integration tests \n"
			"\t-A addr \t sets the _base address of this device \n"
			"\t\t\t this can be a hostname or IP address \n"
			"\t-P port \t sets the port the internal CoAP server is bound to \n"
			"\t-B bb_uri \t changes the preconfigured bulletin board URI \n"
			"\t-R res_name \t sets the resource name for the config link relation\n"
			"\t-n name \t sets the name string of the thing description\n"
			"\t-p purpose \t sets the purpose string of the thing description\n"
			"\t-l location \t sets the location string of the thing description\n\n"
			"Example (defaults): \n"
			"\t./LRC -P 5001 -B \"coap://127.0.0.1:5555\" -R \"config\" -n \"LRC 1\"\n");
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
			state_res_name = optarg;
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
	if(!state_res_name) state_res_name = "state";
	if(port < 0) port = 5001;
	if(!device_addr) device_addr = "127.0.0.1";
	if(!name) name = "LRC 1";
	if(!purpose) purpose = "Controls light 1";
	if(!location) location = "living room";
	base_uri = malloc((18 * sizeof(char)) + strlen(device_addr));
	sprintf(base_uri, "coap://%s:%d", device_addr, port);
	size_t res_name_size = strlen(state_res_name);
	state_res_ref = malloc(res_name_size + sizeof(char) + 1);
	strcpy(state_res_ref, "/");
	strcat(state_res_ref, state_res_name);
	linkobj_t* state_link = malloc(sizeof(linkobj_t));
	*state_link = (linkobj_t) {state_res_ref, "application/lighting+json"};
	links = malloc(sizeof(linkobj_node_t));
	*links = (linkobj_node_t) { .idf = "about", .type=json_type_object, .link = state_link, .next = NULL };
	TD = malloc(sizeof(thing_description_t));
	*TD = (thing_description_t) {.name = name, .purpose = purpose, .location = location};

}

int main(int argc, char* argv[]) {
	/* initial state and defaults */
	char input[20];
	state = &(lighting_t) {
		. on = 0,
		.colorMode = "hs",
		.hue = 300,
		.saturation = 1.0,
		.brightness = 0.5,
		.colorTemperature = 0,
		.x = 0.0,
		.y = 0.0
	};

	handle_cmdline(argc, argv);

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
	init_resource(context, state_res_name);
	/*Listen for incoming connections*/
	info("Interactive LRC. Commands: on, off, brightness, hs, xy, ct \n");
	while (!(can_exit && coap_can_exit(context))) {
		FD_ZERO(&readfds);
		FD_SET(context->sockfd, &readfds);
		fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
		int result = select( FD_SETSIZE, &readfds, 0, 0, &recieve_timer);
		if (result < 0) {
			/* socket error */
			debug("socket err\n");
			exit(EXIT_FAILURE);
		} else if (result > 0) {
			/* socket read*/
			if(FD_ISSET(context->sockfd, &readfds)) {
				coap_read(context);
			}
		} else {
			extern int bulletinboard_register_success;
			if(!bulletinboard_register_success) {
				info("failed registering at bulletin board!\n");
				exit(1);
			}
			cli_modify_state(input);
			recieve_timer.tv_sec = RESPONSE_TIMER_SEC;
			recieve_timer.tv_usec = 0;
		}
		coap_check_notify(context);
	}
	if(test_mode) sleep(1);
	coap_free_context(context);
	free(state_res_ref);
	free(base_uri);
	free(links->link);
	free(links);
	free(TD);
	return 0;
}
