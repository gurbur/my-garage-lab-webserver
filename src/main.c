#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/epoll.h>
#include <linux/limits.h>

#include "config.h"
#include "logger.h"
#include "server.h"
#include "worker.h"
#include "connection.h"

static volatile sig_atomic_t running = 1;

static void signal_handler(int signum) {
	(void)signum;
	running = 0;
}

int main() {
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGPIPE, SIG_IGN);

	server_config config;
	config_init_defaults(&config);

	if (load_config("server.conf", &config) != 0) {
		fprintf(stderr, "Failed to load configuration, using defaults.");
	}

	char absolute_doc_root[PATH_MAX];
	if (realpath(config.document_root, absolute_doc_root) == NULL) {
		fprintf(stderr, "Invalid document_root: %s\n", config.document_root);
		perror("realpath failed");
		free_config(&config);
		return 1;
	}

	free(config.document_root);
	config.document_root = strdup(absolute_doc_root);
	if (config.document_root == NULL) {
		fprintf(stderr, "Failed to allocate memory for docuemnt_root\n");
		free_config(&config);
		return 1;
	}

	if (logger_init(config.log_file) != 0) {
		fprintf(stderr, "Failed to initialize logger.\n");
		free_config(&config);
		return 1;
	}
	log_message(NULL, "Server starting...");
	int listen_fd = init_server(&config);
	if (listen_fd < 0) {
		log_message(NULL, "FATAL: Server initialization failed.");
		logger_close();
		free_config(&config);
		return 1;
	}

	pthread_t workers[config.num_workers];
	int pipe_fds[config.num_workers][2];

	log_message(NULL, "Creating %d worker threads...", config.num_workers);
	for (int i = 0; i < config.num_workers; i++) {
		if (pipe(pipe_fds[i]) == -1) {
			log_message(NULL, "FATAL: Failed to create pipe for worker %d", i);

			for (int j = 0; j < i; j++) {
				close(pipe_fds[j][0]);
				close(pipe_fds[j][1]);
			}
			return 1;
		}

		worker_init_t* init_data = malloc(sizeof(worker_init_t));
		if (!init_data) {
			log_message(NULL, "FATAL: Failed to malloc for worker_init_t");
			for (int j = 0; j < i; j++) {
				close(pipe_fds[j][0]);
				close(pipe_fds[j][1]);
			}
			return 1;
		}
		init_data->worker_id = i;
		init_data->pipe_read_fd = pipe_fds[i][0];
		init_data->config = &config;

		if (pthread_create(&workers[i], NULL, worker_thread_main, init_data) != 0) {
			log_message(NULL, "FATAL: Failed to create worker thread %d", i);
			free(init_data);
			// need cleanup logic
			return 1;
		}
	}

	log_message(NULL, "Main thread is now running as an Acceptor.");
	printf("Server is running. Press Ctrl+C to exit.\n");

	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);

	int epoll_fd = epoll_create1(0);
	struct epoll_event event, events[1];

	event.events = EPOLLIN;
	event.data.fd = listen_fd;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event);

	int next_worker = 0;
	while(running) {
		int n_events = epoll_wait(epoll_fd, events, 1, 1000);
		if (n_events < 0) {
			if (errno == EINTR) continue;
			break;
		}

		if (n_events > 0) {
			int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &addr_len);
			if (client_fd < 0) {
				if (errno == EINTR && !running) break;
				if (errno == EINTR) continue;
				log_message(NULL, "ERROR: accept() failed in main loop: %s", strerror(errno));
				continue;
			}

			connection_t* conn = malloc(sizeof(connection_t));
			if (!conn) {
				log_message(NULL, "ERROR: malloc for connection_t failed");
				close(client_fd);
				continue;
			}
			conn->fd = client_fd;
			conn->timer_node = NULL;

			if (client_addr.ss_family == AF_INET) {
				inet_ntop(AF_INET, &(((struct sockaddr_in*)&client_addr)->sin_addr), conn->client_ip, INET_ADDRSTRLEN);
			} else {
				inet_ntop(AF_INET6, &(((struct sockaddr_in6*)&client_addr)->sin6_addr), conn->client_ip, INET_ADDRSTRLEN);
			}

			int pipe_write_fd = pipe_fds[next_worker][1];
			if (write(pipe_write_fd, &conn, sizeof(connection_t*)) < 0) {
				log_message(conn->client_ip, "ERROR: Failed to dispatch fd %d to worker %d", client_fd, next_worker);
				free(conn);
				close(client_fd);
			} else {
				log_message(conn->client_ip, "Main: Dispatched fd %d to worker %d", client_fd, next_worker);
			}

			next_worker = (next_worker + 1) % config.num_workers;
		}
	}

	printf("\nCtrl+C triggered. Terminating server...\n");
	log_message(NULL, "Server shutting down...");

	for (int i = 0; i < config.num_workers; i++) {
		close(pipe_fds[i][1]);
	}

	for (int i = 0; i < config.num_workers; i++) {
		pthread_join(workers[i], NULL);
		log_message(NULL, "Worker thread %d joined.", i);
	}

	close(listen_fd);
	free_config(&config);
	log_message(NULL, "Server shutdown complete.");
	logger_close();

	return 0;
}
