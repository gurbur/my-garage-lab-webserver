#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "http.h"
#include "logger.h"

static const char* get_mime_type(const char* filename) {
	if (strstr(filename, ".html")) return "text/html";
	if (strstr(filename, ".css")) return "text/css";
	if (strstr(filename, ".js")) return "application/javascript";
	if (strstr(filename, ".png")) return "image/png";
	if (strstr(filename, ".jpg")) return "image/jpeg";
	if (strstr(filename, ".gif")) return "image/gif";
	return "application/octet-stream";
}

int parse_http_request(char *buffer, http_request_t *req) {
	char* saveptr;
	char* method = strtok_r(buffer, " ", &saveptr);
	if (!method) return -1;
	req->method = strdup(method);

	char* uri = strtok_r(NULL, " ", &saveptr);
	if (!uri) return -1;
	req->uri = strdup(uri);

	if (strstr(req->uri, "..")) {
		return -1;
	}

	return 0;
}

void free_http_request(http_request_t *req) {
	free(req->method);
	free(req->uri);
}

void send_error_response(int client_fd, int status_code) {
	const char* status_message;
	char body[128];
	char response[256];

	switch (status_code) {
		case 400: status_message = "Bad Request"; break;
		case 403: status_message = "Forbidden"; break;
		case 404: status_message = "Not Found"; break;
		case 405: status_message = "Method Not Allowed"; break;
		default: status_message = "Internal Server Error"; break;
	}

	sprintf(body, "<html><body><h1>%d %s</h1></body></html>", status_code, status_message);

	sprintf(response,
			"HTTP/1.1 %d %s\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %ld\r\n"
			"X-Content-Type-Options: nosniff\r\n"
			"X-Frame-Options: DENY\r\n"
			"Connection: close\r\n\r\n%s",
			status_code, status_message, strlen(body), body);

	write(client_fd, response, strlen(response));
}

int serve_static_file(int client_fd, const char *request_uri, server_config *config) {
	char filepath[256];

	if (strcmp(request_uri, "/") == 0) {
		snprintf(filepath, sizeof(filepath), "%s/index.html", config->document_root);
	} else if (strncmp(request_uri, "/images/", 8) == 0 || strncmp(request_uri, "/static/", 8) == 0) {
		snprintf(filepath, sizeof(filepath), "%s%s", config->document_root, request_uri);
	} else {
		const char* last_dot = strrchr(request_uri, '.');
		if (last_dot && (strcmp(last_dot, ".html") == 0 || strcmp(last_dot, ".css") == 0 || strcmp(last_dot, ".js") == 0)) {
			snprintf(filepath, sizeof(filepath), "%s%s", config->document_root, request_uri);
		} else {
			snprintf(filepath, sizeof(filepath), "%s%s.html", config->document_root, request_uri);
		}
	}

	char real_filepath[PATH_MAX];
	if (realpath(filepath, real_filepath) == NULL) {
		log_message(NULL, "INFO: File not found for URI '%s', mapped to '%s'", request_uri, filepath);
		send_error_response(client_fd, 404);
		return -1;
	}

	if (strncmp(real_filepath, config->document_root, strlen(config->document_root)) != 0) {
		log_message(NULL, "WARN: Path Traversal attempt blocked. URI: '%s', Resolved: '%s'", request_uri, real_filepath);
		send_error_response(client_fd, 403);
		return -1;
	}

	struct stat file_stat;
	if (stat(real_filepath, &file_stat) < 0) {
		log_message("ERROR: stat error for %s: %s", real_filepath, strerror(errno));
		send_error_response(client_fd, 500);
		return -1;
	}

	if (!S_ISREG(file_stat.st_mode)) {
		send_error_response(client_fd, 403);;
		log_message("DEBUG: Not a regular file. Returning -1.");
		return -1;
	}

	int file_fd = open(real_filepath, O_RDONLY);
	if (file_fd < 0) {
		send_error_response(client_fd, 403);
		log_message("DEBUG: Permission denied. Returning -1.");
		return -1;
	}

	char header[256];
	const char* mime_type = get_mime_type(real_filepath);

	sprintf(header,
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: %s\r\n"
			"Content-Length: %ld\r\n"
			"X-Content-Type-Options: nosniff\r\n"
			"X-Frame-Options: DENY\r\n"
			"Connection: keep-alive\r\n\r\n",
			mime_type, file_stat.st_size);

	write(client_fd, header, strlen(header));

	char buffer[4096];
	ssize_t bytes_read;
	while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
		ssize_t bytes_written = 0;
		while (bytes_written < bytes_read) {
			ssize_t result = write(client_fd, buffer + bytes_written, bytes_read - bytes_written);
			if (result < 0) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) {
					continue;
				}
				log_message("ERROR: Failed to write to socket: %s", strerror(errno));
				close(file_fd);
				return -1;
			}
			bytes_written += result;
		}
	}

	close(file_fd);
	log_message("DEBUG: File sent successfully. Returning 0.");
	return 0;
}

