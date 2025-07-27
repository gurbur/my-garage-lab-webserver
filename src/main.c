#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

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
	log_message("Server starting in multi-threaded mode...");
	listen_fd = init_server(&config);
	if (listen_fd < 0) {
		log_message("FATAL: Server initialization failed.");
		logger_close();
		free_config(&config);
		return 1;
	}

	log_message("Main thread is now running as an Acceptor.");
	printf("Server is running. Press Ctrl+C to exit.\n");

	while(1) {
		struct sockaddr_in client_addr;
		socklen_t client_len = sizeof(client_addr);

		int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

		if (client_fd < 0) {
			log_message("ERROR: accept() failed in main loop");
			continue;
		}

		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
		log_message("Accepted new connection from %s on fd %d", client_ip, client_fd);

		// TODO: send client_fd to worker thread.
		// currently, there is no worker thread,
		// get new connection, leave log, and close promptly.

		close(client_fd);
	}

	close(listen_fd);
	logger_close();
	free_config(&config);

	return 0;
}
