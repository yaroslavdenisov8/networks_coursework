#ifndef LOGGER_H
#define LOGGER_H

int logger_open(const char *path);
void logger_close(void);
void log_event(const char *format, ...);

#endif
