#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>

#include "logger.h"
#include "worker.h"

#define MAX_EVENTS 64
#define READ_BUFFER_SIZE 1024

static int make_socket_non_blocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) {
		log_message("ERROR: fcntl(F_GETFL) failed");
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
	worker_init_t* init_data = (worker_init_t*) arg;
	int worker_id = init_data->worker_id;
	int pipe_read_fd = init_data->pipe_read_fd;
	free(init_data);

	int epoll_fd;
	struct epoll_event event, events[MAX_EVENTS];

	epoll_fd = epoll_create1(0);
	if (epoll_fd == -1) {
		log_message("FATAL: Worker %d: epoll_create1 failed", worker_id);
		return NULL;
	}

	event.events = EPOLLIN;
	event.data.fd = pipe_read_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipe_read_fd, &event) == -1) {
		log_message("FATAL: Worker %d: epoll_ctl failed to add pipe", worker_id);
		close(epoll_fd);
		return NULL;
	}

	log_message("Worker %d started successfully.", worker_id);

	while (1) {
		int n_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);

		if (n_events < 0) {
			if (errno == EINTR) continue;
			log_message("ERROR: Worker %d: epoll_wait failed", worker_id);
			break;
		}

		for (int i = 0; i < n_events; i++) {
			if (events[i].data.fd == pipe_read_fd) {
				int client_fd;
				ssize_t bytes_read = read(pipe_read_fd, &client_fd, sizeof(client_fd));

				if (bytes_read == sizeof(client_fd)) {
					log_message("Worker %d: Received new job (fd: %d)", worker_id, client_fd);
					make_socket_non_blocking(client_fd);
					event.data.fd = client_fd;
					event.events = EPOLLIN | EPOLLET;
					if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
						log_message("ERROR: Worker %d: epoll_ctl failed to add client fd %d", worker_id, client_fd);
						close(client_fd);
					}
				} else if (bytes_read <= 0) {
					log_message("FATAL: Worker %d: Pipe closed or error. Worker thread terminating.", worker_id);
					close(pipe_read_fd);
					close(epoll_fd);
					return NULL;
				}
			} else {
				int client_fd = events[i].data.fd;
				bool should_close = false;
				bool data_read_successfully = false;

				while (1) {
					char buffer[READ_BUFFER_SIZE];
					ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

					if (bytes_read > 0) {
						data_read_successfully = true;
					} else if (bytes_read == 0) {
						should_close = true;
						break;
					} else {
						if (errno == EAGAIN || errno == EWOULDBLOCK) {
							break;
						} else {
							log_message("Worker %d: Error reading from fd %d", worker_id, client_fd);
							should_close = true;
							break;
						}
					}
				}

				if (data_read_successfully) {
					const char* http_response = 
						"HTTP/1.1 200 OK\r\n"
						"Content-Type: text/plain; charset=utf-8\r\n"
						"Content-Length: 13\r\n"
						"\r\n"
						"Hello, World!";

						if(write(client_fd, http_response, strlen(http_response)) < 0) {
							log_message("ERROR: Worker %d: Failed to write to fd %d", worker_id, client_fd);
						} else {
							log_message("Worker %d: Sent response to fd %d", worker_id, client_fd);
						}
						should_close = true;
				}

				if (should_close) {
					epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
					close(client_fd);
					log_message("Worker %d: Closed connection on fd %d", worker_id, client_fd);
				}
			}
		}
	}

	close(epoll_fd);
	return NULL;
}

