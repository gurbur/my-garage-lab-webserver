#pragma once

#include "timer.h"

typedef struct connection_s {
	int fd;
	timer_node_t* timer_node;
} connection_t;

