/*
 * coap-helper.c
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "coap.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>

/**
 * Sets the dst_addr to server_uri. Tries to resolve the hostname.
 * Return 1 on success or 0 on error.
 */
int cl_set_addr(coap_uri_t *server_uri, coap_address_t *dst_addr) {
	if (!server_uri || !dst_addr)
		return 0;

	/* create host string */
	char* host = malloc(server_uri->host.length + 1);
	if(!host) exit(1);
	memcpy(host, server_uri->host.s, server_uri->host.length);
	host[server_uri->host.length] = '\0';
	if (!host)
		return 0;

	/* create port string */
	 int psize = snprintf(NULL, 0, "%d", server_uri->port);
		  if (psize < 1) exit(1);
		  char *port = malloc(psize + 1);
		  if (!port) exit(1);
		  sprintf(port, "%d", server_uri->port);

	struct addrinfo supported;
	struct addrinfo *result, *ainfo;

	 memset(&supported, 0, sizeof(struct addrinfo));
	  supported.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
	  supported.ai_socktype = SOCK_DGRAM; /* CoAP uses UDP */

	  int ret = getaddrinfo(host, port, &supported, &result);
	  if ( ret != 0 ) {
		  info("getaddrinfo failed with code: %s\n", gai_strerror(ret));
	    exit(1);
	  }
	  free(host);
	  free(port);

	  /* iterate through addrinfos and pick first match */
	  for (ainfo = result; ainfo != NULL; ainfo = ainfo->ai_next) {
		int len = -1;
		switch (ainfo->ai_family) {
		  case AF_INET6:
		  case AF_INET:
			len = ainfo->ai_addrlen;
			coap_address_init(dst_addr);
			memcpy(&dst_addr->addr.sa, ainfo->ai_addr, len);
			freeaddrinfo(result);
			return 1;
		  default:
			;
		  }
	  }
	  return 0;
}

/**
 * Sets the src_addr to 0.0.0.0:0.
 */
void cl_server_addr(coap_address_t *src_addr) {
	coap_address_init(src_addr);
	src_addr->addr.sin.sin_family = AF_INET;
	src_addr->addr.sin.sin_port = htons(0);
	src_addr->addr.sin.sin_addr.s_addr = inet_addr("0.0.0.0");
}

/*
 * Return 1 if given request has content_type option equal to given coap media type.
 * Returns 0 otherwise.
 */
int cl_media_type_equals(coap_pdu_t** request, int coap_media_type) {
	coap_opt_iterator_t oi;
	coap_opt_t* conent_type_opt = coap_check_option(*request,
			COAP_OPTION_CONTENT_FORMAT, &oi);

	if (conent_type_opt == NULL) {
		debug("no media type to check\n");
		return 0;
	} else {
		coap_option_t content_type_bytes;
		coap_opt_parse(conent_type_opt, 6, &content_type_bytes);
		unsigned int content_type = coap_decode_var_bytes(
				content_type_bytes.value, content_type_bytes.length);
		debug("found media_type %d\n", content_type);
		if (content_type == coap_media_type) {
			return 1;
		} else {
			return 0;
		}
	}
}

/*
 * Returns a copy of the given request data as char*
 */
char* cl_copy_reqdata(unsigned char** dataptr, size_t size) {
	char* reqdata = (char*) malloc((size + 1) * sizeof(char));
	memcpy(reqdata, (char*) *dataptr, size);
	reqdata[size] = '\0';
	return reqdata;
}
