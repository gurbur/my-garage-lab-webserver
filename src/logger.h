#pragma once

#include <stdarg.h>

int logger_init(const char* filename);
void log_message(const char* client_ip, const char* format, ...);
void logger_close();

