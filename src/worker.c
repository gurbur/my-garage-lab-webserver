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
#include "connection.h"
#include "timer.h"
#include "http.h"

#define MAX_EVENTS 64
#define REQUEST_BUFFER_SIZE 8192
#define CONNECTION_TIMEOUT 60

typedef struct {
	int worker_id;
	int epoll_fd;
	timer_wheel_t* tw;
	server_config* config;
} worker_context_t;

static int make_socket_non_blocking(int fd);
static void close_connection(worker_context_t* ctx, connection_t* conn);
static void handle_client_event(worker_context_t* ctx, connection_t* conn);
static void handle_pipe_event(worker_context_t* ctx, int pipe_read_fd);

void* worker_thread_main(void* arg) {
	worker_init_t* init_data = (worker_init_t*) arg;

	worker_context_t ctx = {
		.worker_id = init_data->worker_id,
		.config = init_data->config
	};

	int pipe_read_fd = init_data->pipe_read_fd;
	int pipe_write_fd = init_data->pipe_write_fd;
	free(init_data);
	close(pipe_write_fd);

	ctx.tw = timer_wheel_create(60, 1);
	ctx.epoll_fd = epoll_create1(0);
	if (!ctx.tw || ctx.epoll_fd == -1) {
		log_message("FATAL: Worker %d: timer_wheel_create failed", ctx.worker_id);
		if (ctx.tw) timer_wheel_destroy(ctx.tw);
		if (ctx.epoll_fd != -1) close(ctx.epoll_fd);
		return NULL;
	}

	struct epoll_event event, events[MAX_EVENTS];
	event.events = EPOLLIN;
	event.data.fd = pipe_read_fd;
	epoll_ctl(ctx.epoll_fd, EPOLL_CTL_ADD, pipe_read_fd, &event);

	log_message("Worker %d started successfully.", ctx.worker_id);

	while (1) {
		int n_events = epoll_wait(ctx.epoll_fd, events, MAX_EVENTS, 1000);
		if (n_events < 0) {
			if (errno == EINTR) continue;
			break;
		}

		for (int i = 0; i < n_events; i++) {
			if (events[i].data.fd == pipe_read_fd) {
				handle_pipe_event(&ctx, pipe_read_fd);
			} else {
				handle_client_event(&ctx, (connection_t*)events[i].data.ptr);
			}
		}

		timer_node_t* expired_list = timer_wheel_tick(ctx.tw);
		while (expired_list) {
			timer_node_t* current_node = expired_list;
			expired_list = expired_list->next;
			close_connection(&ctx, (connection_t*)current_node->conn);
			log_message("Worker %d: Closing connection due to timeout", ctx.worker_id);
		}
	}

	log_message("Worker %d terminating.", ctx.worker_id);
	close(pipe_read_fd);
	close(ctx.epoll_fd);
	timer_wheel_destroy(ctx.tw);
	return NULL;
}

static void close_connection(worker_context_t* ctx, connection_t* conn) {
	if (!conn) return;
	epoll_ctl(ctx->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
	if (conn->timer_node) {
		timer_node_remove(ctx->tw, conn->timer_node);
	}
	close(conn->fd);
	log_message("Worker %d: Closed connection on fd %d", ctx->worker_id, conn->fd);
	free(conn);
}

static void handle_pipe_event(worker_context_t* ctx, int pipe_read_fd) {
	int client_fd;
	ssize_t bytes_read = read(pipe_read_fd, &client_fd, sizeof(client_fd));

	if (bytes_read == sizeof(client_fd)) {
		connection_t* conn = malloc(sizeof(connection_t));
		if (!conn) {
			close(client_fd);
			return;
		}
		conn->fd = client_fd;
		conn->timer_node = timer_node_add(ctx->tw, conn, CONNECTION_TIMEOUT);

		make_socket_non_blocking(client_fd);

		struct epoll_event event;
		event.data.ptr = conn;
		event.events = EPOLLIN | EPOLLET;
		if (epoll_ctl(ctx->epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
			close_connection(ctx, conn);
		} else {
			log_message("Worker %d: Received new job (fd: %d)", ctx->worker_id, client_fd);
		}
	}
}

static void handle_client_event(worker_context_t* ctx, connection_t* conn) {
	char buffer[REQUEST_BUFFER_SIZE] = {0};
	ssize_t total_bytes_read = 0;
	bool should_close = false;

	while (total_bytes_read < REQUEST_BUFFER_SIZE - 1) {
		ssize_t bytes_read = read(conn->fd, buffer + total_bytes_read, REQUEST_BUFFER_SIZE - total_bytes_read - 1);

		if (bytes_read > 0) {
			total_bytes_read += bytes_read;
		} else if (bytes_read == 0) {
			should_close = true;
			break;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				break;
			} else {
				should_close = true;
				break;
			}
		}
	}

	if (total_bytes_read > 0) {
		http_request_t req = {0};
		if (parse_http_request(buffer, &req) == 0) {
			if (strcmp(req.method, "GET") == 0) {
				if (serve_static_file(conn->fd, req.uri, ctx->config) != 0) {
					should_close = true;
				} else {
					if (conn->timer_node) timer_node_remove(ctx->tw, conn->timer_node);
					conn->timer_node = timer_node_add(ctx->tw, conn, CONNECTION_TIMEOUT);
				}
			} else {
				send_error_response(conn->fd, 405);
				should_close = true;
			}
			free_http_request(&req);
		} else {
			send_error_response(conn->fd,400);
			should_close = true;
		}
	}

	if (should_close) {
		close_connection(ctx, conn);
	}
}

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
