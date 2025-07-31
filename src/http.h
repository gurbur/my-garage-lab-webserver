#pragma once

#include "server.h"

typedef struct {
	char* method;
	char* uri;
} http_request_t;

int parse_http_request(char* buffer, http_request_t* req);
void free_http_request(http_request_t* req);
void send_error_response(int client_fd, int status_code);
int serve_static_file(int client_fd, const char* request_uri, server_config* config);

