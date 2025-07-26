#pragma once

typedef struct {
	int port;
	int num_workers;
	char *document_root;
	char* log_file;
} server_config;

int load_config(const char* filename, server_config* config);
void free_config(server_config* config);

