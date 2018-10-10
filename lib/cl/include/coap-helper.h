/*
 * coap-helper.h
 *
 *  Created on: Mar 4, 2017
 *      Author: Nikolas Rösener
 */
#include "coap.h"

#ifndef LIB_CL_INCLUDE_COAP_HELPER_H_
#define LIB_CL_INCLUDE_COAP_HELPER_H_

int cl_set_addr(coap_uri_t *server_uri, coap_address_t *dst_addr);

void cl_server_addr(coap_address_t *src_addr);

int cl_media_type_equals(coap_pdu_t** request, int coap_media_type);

char* cl_copy_reqdata(unsigned char** dataptr, size_t size);

#endif /* LIB_CL_INCLUDE_COAP_HELPER_H_ */
