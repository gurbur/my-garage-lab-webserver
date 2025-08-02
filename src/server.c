#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#include "server.h"
#include "logger.h"

int init_server(server_config *config) {
	int listen_fd;
	struct sockaddr_in server_addr;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		log_message("ERROR: socket() failed: %s", strerror(errno));
		return -1;
	}

	int opt = 1;
	if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		log_message("ERROR: setsockopt(SO_REUSEADDR) failed: %s", strerror(errno));
		close(listen_fd);
		return -1;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(config->port);

	if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
		log_message("ERROR: bind() failed on port %d: %s", config->port, strerror(errno));
		close(listen_fd);
		return -1;
	}

	if (listen(listen_fd, SOMAXCONN) < 0) {
		log_message("ERROR: listen() failed: %s", strerror(errno));
		close(listen_fd);
		return -1;
	}

	log_message("Server listening on port %d", config->port);
	return listen_fd;
}

