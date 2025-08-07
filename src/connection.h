#pragma once

#include <netinet/in.h>
#include "timer.h"

typedef struct connection_s {
	int fd;
	timer_node_t* timer_node;
	char client_ip[INET_ADDRSTRLEN];
} connection_t;

