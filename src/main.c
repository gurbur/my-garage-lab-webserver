#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "config.h"
#include "logger.h"
#include "server.h"
#include "worker.h"

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

	pthread_t workers[config.num_workers];
	log_message("Creating %d worker threads...", config.num_workers);
	for (int i = 0; i< config.num_workers; i++) {
		// TODO: have to send pipe fd to worker
		if (pthread_create(&workers[i], NULL, worker_thread_main, NULL) != 0) {
			log_message("FATAL: Failed to create worker thread %d", i);
			return 1;
		}
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

		// TODO: give this client_fd to worker thread using round-robin

		close(client_fd);
	}

	//for (int i = 0; i < config.num_workers; i++) {
	//pthread_join(workers[i], NULL);
	//}
	close(listen_fd);
	logger_close();
	free_config(&config);

	return 0;
}
