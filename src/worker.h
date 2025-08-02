#pragma once

#include "server.h"

typedef struct {
	int worker_id;
	int pipe_read_fd;
	int pipe_write_fd;
	server_config* config;
} worker_init_t;

void* worker_thread_main(void* arg);

