#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>

#include "logger.h"

static FILE* log_file = NULL;

int logger_init(const char* filename) {
	log_file = fopen(filename, "a");
	if (!log_file) {
		perror("Error: could not open log file");
		return -1;
	}
	return 0;
}

void log_message(const char *format, ...) {
	if (!log_file) {
		fprintf(stderr, "Logger not initialized.\n");
		return;
	}

	time_t now = time(NULL);
	struct tm t;
	char time_buf[20];

	localtime_r(&now, &t);
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &t);

	fprintf(log_file, "[%s] ", time_buf);

	va_list args;
	va_start(args, format);
	vfprintf(log_file, format, args);
	va_end(args);

	fprintf(log_file, "\n");

	fflush(log_file);
}

void logger_close() {
	if (log_file) {
		fclose(log_file);
		log_file = NULL;
	}
}

