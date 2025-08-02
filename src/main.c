#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>

#include "config.h"
#include "logger.h"
#include "server.h"
#include "worker.h"

static volatile sig_atomic_t running = 1;

static void signal_handler(int signum) {
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

	if (logger_init(config.log_file) != 0) {
		fprintf(stderr, "Failed to initialize logger.\n");
		free_config(&config);
		return 1;
	}
	log_message("Server starting...");
	int listen_fd = init_server(&config);
	if (listen_fd < 0) {
		log_message("FATAL: Server initialization failed.");
		logger_close();
		free_config(&config);
		return 1;
	}

	pthread_t workers[config.num_workers];
	int pipe_fds[config.num_workers][2];

	log_message("Creating %d worker threads...", config.num_workers);
	for (int i = 0; i < config.num_workers; i++) {
		if (pipe(pipe_fds[i]) == -1) {
			log_message("FATAL: Failed to create pipe for worker %d", i);

			for (int j = 0; j < i; j++) {
				close(pipe_fds[j][0]);
				close(pipe_fds[j][1]);
			}
			return 1;
		}

		worker_init_t* init_data = malloc(sizeof(worker_init_t));
		if (!init_data) {
			log_message("FATAL: Failed to malloc for worker_init_t");
			for (int j = 0; j < i; j++) {
				close(pipe_fds[j][0]);
				close(pipe_fds[j][1]);
			}
			return 1;
		}
		init_data->worker_id = i;
		init_data->pipe_read_fd = pipe_fds[i][0];
		init_data->pipe_write_fd = pipe_fds[i][1];
		init_data->config = &config;

		if (pthread_create(&workers[i], NULL, worker_thread_main, init_data) != 0) {
			log_message("FATAL: Failed to create worker thread %d", i);
			free(init_data);
			// need cleanup logic
			return 1;
		}
	}

	// read-end closing(rollback if not work correctly
	for (int i = 0; i < config.num_workers; i++) {
		close(pipe_fds[i][0]);
	}

	log_message("Main thread is now running as an Acceptor.");
	printf("Server is running. Press Ctrl+C to exit.\n");

	int next_worker = 0;
	while(running) {
		int client_fd = accept(listen_fd, NULL, NULL);
		if (client_fd < 0) {
			if (errno == EINTR && !running) break;
			if (errno == EINTR) continue;
			log_message("ERROR: accept() failed in main loop: %s", strerror(errno));
			continue;
		}

		int pipe_write_fd = pipe_fds[next_worker][1];
		if (write(pipe_write_fd, &client_fd, sizeof(client_fd)) < 0) {
			log_message("ERROR: Failed to dispatch fd %d to worker %d", client_fd, next_worker);
			close(client_fd);
		} else {
			log_message("Main: Dispatched fd %d to worker %d", client_fd, next_worker);
		}

		next_worker = (next_worker + 1) % config.num_workers;
	}

	log_message("Server shutting down...");

	for (int i = 0; i < config.num_workers; i++) {
		close(pipe_fds[i][1]);
	}

	for (int i = 0; i < config.num_workers; i++) {
		pthread_join(workers[i], NULL);
		log_message("Worker thread %d joined.", i);
	}

	close(listen_fd);
	logger_close();
	free_config(&config);
	log_message("Server shutdown complete.");

	return 0;
}
