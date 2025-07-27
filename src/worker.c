#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>

#include "logger.h"
#include "worker.h"

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

void* worker_thread_main(void* arg) {
	// long worker_id = (long)arg;
	int epoll_fd;
	struct epoll_event events[MAX_EVENTS];

	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		log_message("FATAL: epoll_create1 failed");
		return NULL;
	}

	//log_message("Worker %ld started.", worker_id);

	// TODO: register pipe's read-end on epoll to get client_fd from Main thread

	while (1) {
		int n_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (n_events == -1) {
			log_message("ERROR: epoll_wait failed");
			continue;
		}

		for (int i = 0; i < n_events; i++) {
			// TODO: add logic to get new client_fd from Main thread

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
				log_message("Worker sent fixed response to fd %d", client_fd);
			}

			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
			close(client_fd);
			log_message("Closed connection on fd %d", client_fd);

		}
	}

	close(epoll_fd);
	return NULL;
}

