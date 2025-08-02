#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"

#define LINE_MAX_LEN 256
#define KEY_MAX_LEN 64
#define VALUE_MAX_LEN 192

static void trim_whitespace(char* str) {
	char* start = str;
	while (isspace((unsigned char)*start)) {
		start++;
	}

	char* end = str + strlen(str) - 1;
	while (end > start && isspace((unsigned char)*end)) {
		end--;
	}
	*(end + 1) = '\0';

	if (start > str) {
		memmove(str, start, strlen(start) + 1);
	}
}

void config_init_defaults(server_config* config) {
	config->port = 8080;
	config->num_workers = 4;
	config->document_root = strdup("./ssg_output");
	config->log_file = strdup("server.log");
}

int load_config(const char *filename, server_config *config) {
	FILE* file = fopen(filename, "r");
	if (!file) {
		perror("Error: could not open config file");
		return -1;
	}

	char line[LINE_MAX_LEN];
	while (fgets(line, sizeof(line), file)) {
		if (line[0] == '#' || line[0] == '\n') {
			continue;
		}

		char key[KEY_MAX_LEN], value[VALUE_MAX_LEN];

		if (sscanf(line, " %63[^= ] = %191[^\n]", key, value) != 2) continue;

		trim_whitespace(key);
		trim_whitespace(value);

		if (strcmp(key, "port") == 0) {
			config->port = atoi(value);
		} else if (strcmp(key, "num_workers") == 0) {
			config->num_workers = atoi(value);
		} else if (strcmp(key, "document_root") == 0) {
			free(config->document_root);
			config->document_root = strdup(value);
			if (!config->document_root) {
				perror("Error: strdup failed for document_root");
				fclose(file);
				return -1;
			}
		} else if (strcmp(key, "log_file") == 0) {
			free(config->log_file);
			config->log_file = strdup(value);
			if (!config->log_file) {
				perror("Error: strdup failed for log_file");
				fclose(file);
				return -1;
			}
		}
	}

	fclose(file);
	return 0;
}

void free_config(server_config *config) {
	if (config) {
		free(config->document_root);
		free(config->log_file);
	}
}

