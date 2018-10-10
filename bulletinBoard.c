/*
 * bulletinBoard.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "coap.h"
#include "coap-helper.h"
#include "core-lighting.h"

#include <time.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

/* the maximum size of a bulletin-board representation */
#define BULLETIN_MAX_SIZE 512
/* maximum length of embedded bulletins-list */
#define BULLETINS_MAX_LEN 20
/* defines the receive loop interval */
#define RESPONSE_TIMER_USEC 200
/* name of the resource holding the state */
#define BULLETINS_RES_NAME "bulletins"
/* application log level */
#define LOG_LEVEL LOG_INFO

/* options modifyable by cmdline arguments */
char* bulletins_res_name = NULL;
char* bulletins_res_ref = NULL;
char* base_uri = NULL;
int port = -1;
char* device_addr = NULL;
int test_mode = 0;
int can_exit = 0;

struct coap_resource_t *bulletins_resource = NULL;
json_object* bulletins[BULLETINS_MAX_LEN] = { NULL };
size_t bulletins_size = 0;

/**
 * bulletins resource GET handler
 */
static void bulletins_get_request_handler(coap_context_t *context, struct coap_resource_t *resource,
		const coap_endpoint_t *local_interface, coap_address_t *peer, coap_pdu_t *request, str *token,
		coap_pdu_t *response) {
	unsigned char buffer[3]; //max size of options
	formobj_t form = {
			.href = bulletins_res_ref,
			.method = "POST",
			.accept = "application/thing-description+json"
	};
	formobj_node_t forms = {.idf = "create-item", .type = json_type_object, .form = &form};
	resobj_array_t ro_array = {.size = bulletins_size, .roptr = bulletins};
	embdobj_node_t eol = { .idf = "item", .type = json_type_array, .embedded_arr = &ro_array };
	resobj_t ro = {.base = base_uri, .forms = &forms, .embedded = &eol};

	unsigned char* res = NULL;
	size_t len = create_response(&ro, &res);
	debug("Sending response (%d byte): %s\n", len, res);

	response->hdr->code = COAP_RESPONSE_CODE(205);
	if (coap_find_observer(resource, peer, token)) {
		coap_add_option(response, COAP_OPTION_OBSERVE, coap_encode_var_bytes(buffer, context->observe), buffer);
	}
	coap_add_option(response, COAP_OPTION_CONTENT_FORMAT,
			coap_encode_var_bytes(buffer, APPLICATION_BULLETIN_BOARD_JSON), buffer);

	coap_add_data(response, len, res);

	free(res);

	if(test_mode > 2) can_exit = 1;
}

static void bulletins_post_request_handler(coap_context_t *ctx, struct coap_resource_t *resource,
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
	if (size == 0) { /* no payload or error */
		debug("no payload \n");
		/* 4.00 client error: empty request not valid */
		response->hdr->code = COAP_RESPONSE_CODE(400);
	} else if (size >= BULLETIN_MAX_SIZE) {
		debug("bulletin too big: %d \n", size);
		/* 4.13 request entity too large */
		response->hdr->code = COAP_RESPONSE_CODE(413);
	} else if (bulletins_size >= BULLETINS_MAX_LEN) {
		debug("bulletins full \n");
		/* 5.00 internal server error: bulletins resource full */
		response->hdr->code = COAP_RESPONSE_CODE(500);
	} else if( !cl_media_type_equals(&request, APPLICATION_THINGDESCRIPTION_JSON)){
		/* 4.15 unsupported content-format */
		response->hdr->code = COAP_RESPONSE_CODE(415);
	} else {
		json_object* root = string_to_json(data, size);
		if(root != NULL) {
			bulletins[bulletins_size] = root;
			bulletins_size++;
			response->hdr->code = COAP_RESPONSE_CODE(204);
			resource->dirty = 1;
			if(test_mode) test_mode++;
		} else {
			response->hdr->code = COAP_RESPONSE_CODE(400);
			info("Error: failed to store bulletin for request\n");
		}
	}
}

static void init_resource(coap_context_t *ctx, char* res_name) {
	coap_resource_t* resource;
	resource = coap_resource_init((const unsigned char*) "", 0, COAP_RESOURCE_FLAGS_NOTIFY_CON);
	coap_register_handler(resource, COAP_REQUEST_GET, bulletins_get_request_handler);
	coap_add_resource(ctx, resource);
	resource = coap_resource_init((const unsigned char*) res_name, strlen(res_name), COAP_RESOURCE_FLAGS_NOTIFY_CON);
	coap_register_handler(resource, COAP_REQUEST_POST, bulletins_post_request_handler);
	coap_add_resource(ctx, resource);
	bulletins_resource = resource;
}

static void handle_cmdline(int argc, char* argv[]) {
	int option = -1;

	while ((option = getopt(argc, argv, "hTA:P:R:B:n:p:l")) != -1) {
		switch (option) {
		case 'h':
			printf(	"Usage: bulletinBoard [-h] [-T] [-A addr] [-P port] [-R res_name]\n\n"
					"Arguments: \n"
					"\t-h \t\t displays this help message \n"
					"\t-T \t\t enables a special test mode used in integration tests \n"
					"\t-A addr \t sets the _base address of this device \n"
					"\t\t\t this can be a hostname or IP address \n"
					"\t-P port \t sets the port the internal CoAP server is bound to \n"
					"\t-R res_name \t sets the resource name for the create-item form target\n\n"
					"Example (defaults): \n"
					"\t./bulletinBoard -A \"127.0.0.1\" -P 5555 -R \"bulletins\"\n");
			exit(0);
		case 'T':
			test_mode = 1;
			break;
		case 'A':
			device_addr = optarg;
			break;
		case 'P':
			port = strtol(optarg, NULL, 10);
			break;
		case 'R':
			bulletins_res_name = optarg;
			break;
		case '?':
			if (optopt == 'P' || optopt == 'R' || optopt == 'A')
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
	if(port < 0) port = 5555;
	if(!device_addr) device_addr = "127.0.0.1";
	if(!bulletins_res_name) bulletins_res_name = "bulletins";
	base_uri = malloc((18 * sizeof(char)) + strlen(device_addr));
	sprintf(base_uri, "coap://%s:%d", device_addr, port);
	bulletins_res_ref = malloc(strlen(bulletins_res_name) + 2);
	strcpy(bulletins_res_ref, "/");
	strcat(bulletins_res_ref, bulletins_res_name);
}

int main(int argc, char* argv[]) {
	handle_cmdline(argc, argv);
	coap_set_log_level(LOG_LEVEL);
	info("core-lighting bulletin board on port %d\n", port);

	/* dynamic CoAP variables */
	coap_context_t* context;
	coap_address_t serv_addr;
	fd_set readfds;
	struct timeval recieve_timer;
	recieve_timer.tv_sec = 0;
	recieve_timer.tv_usec = RESPONSE_TIMER_USEC;

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
	init_resource(context, bulletins_res_name);

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
			recieve_timer.tv_sec = 0;
			recieve_timer.tv_usec = RESPONSE_TIMER_USEC;
		}
		coap_check_notify(context);
	}
	if(test_mode) sleep(1);
	coap_free_context(context);
	free(bulletins_res_ref);
	return 0;
}
