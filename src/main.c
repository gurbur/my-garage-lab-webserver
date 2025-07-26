#include <stdio.h>
#include <unistd.h>

#include "config.h"
#include "logger.h"
#include "server.h"

int main() {
	server_config config;
	int listen_fd;

	if (load_config("server.conf", &config) != 0) {
		fprintf(stderr, "Failed to load configuration.\n");
		return 1;
	}

	if (logger_init(config.log_file) != 0) {
		fprintf(stderr, "Failed to initialize logger.\n");
		free_config(&config);
		return 1;
	}

	log_message("Server starting...");
	log_message(" - Port: %d", config.port);
	log_message(" - Workers: %d", config.num_workers);
	log_message(" - Document Root: %s", config.document_root);
	log_message(" - Log File: %s", config.log_file);

	listen_fd = init_server(&config);
	if (listen_fd < 0) {
		log_message("FATAL: Server initialization failed.");
		logger_close();
		free_config(&config);
		return 1;
	}

	printf("Server initialized successfully. Listening socket created (fd=%d).\n", listen_fd);
	printf("For now, the server will shut down. The event loop will be added next.\n");

	log_message("Server shutting down.");
	close(listen_fd);
	logger_close();
	free_config(&config);

	return 0;
}

