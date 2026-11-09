#define _DARWIN_C_SOURCE
#include "logger.h"
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static int log_fd = -1;

int logger_open(const char *path) {
    log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0640);
    return log_fd < 0 ? -1 : 0;
}

void logger_close(void) {
    if (log_fd >= 0) 
        close(log_fd);
    log_fd = -1;
}

void log_event(const char *format, ...) {
    if (log_fd < 0) 
        return;

    char message[2048], line[2300], timestamp[64];
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_now);

    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    int length = snprintf(line, sizeof(line), "[%s] pid=%ld %s\n",
                          timestamp, (long)getpid(), message);
    if (length > 0) 
        (void)write(log_fd, line, (size_t)length);
}
