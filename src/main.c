#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>

#include "config.h"
#include "logger.h"
#include "server.h"

#define MAX_EVENTS 64
#define READ_BUFFER_SIZE 1024

static int make_socket_non_blocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		log_message("ERROR: fctnl(F_GETFL) failed");
		return -1;
	}

	flags |= O_NONBLOCK;
	if (fcntl(fd, F_SETFL, flags) == -1) {
		log_message("ERROR: fcntl(F_SETFL) failed");
		return -1;
	}
	return 0;
}

int main() {
	server_config config;
	int listen_fd, epoll_fd;

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

	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		log_message("FATAL: epoll_create1 failed");
		close(listen_fd);
		logger_close();
		free_config(&config);
		return 1;
	}

	struct epoll_event event;
	event.data.fd = listen_fd;
	event.events = EPOLLIN | EPOLLET; // incomming connection(EPOLLIN), edge trigger method(EPOLLET)
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1) {
		log_message("FATAL: epoll_ctl failed to add listen_fd");
		close(epoll_fd);
		logger_close();
		free_config(&config);
		return 1;
	}

	struct epoll_event events[MAX_EVENTS];
	log_message("Event loop started. Waiting for connections...");
	printf("Server is running. Press Ctrl+C to exit.\n");

	while (1) {
		int n_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (n_events == -1) {
			log_message("ERROR: epoll_wait failed");
			continue;
		}

		for (int i = 0; i < n_events; i++) {
			if (events[i].data.fd == listen_fd) {
				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);
				int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

				if (client_fd < 0) {
					log_message("ERROR: accept() failed");
					continue;
				}

				char client_ip[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
				log_message("Accepted new connection from %s on fd %d", client_ip, client_fd);

				make_socket_non_blocking(client_fd);
				event.data.fd = client_fd;
				event.events = EPOLLIN | EPOLLET;
				epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event);

			} else {
				int client_fd = events[i].data.fd;

				char buffer[READ_BUFFER_SIZE];
				ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

				if (bytes_read > 0) {
					const char* http_response = 
						"HTTP/1.1 200 OK\r\n"
						"Content-Type: text/plain; charset=utf-8\r\n"
						"Content-Length: 14\r\n"
						"\r\n"
						"Hello, World!";

					write(client_fd, http_response, strlen(http_response));
					log_message("Sent fixed response to fd %d", client_fd);
				}

				epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
				close(client_fd);
				log_message("Closed connection on fd %d", client_fd);

			}
		}
	}

	close(listen_fd);
	close(epoll_fd);
	logger_close();
	free_config(&config);

	return 0;
}

