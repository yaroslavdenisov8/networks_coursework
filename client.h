#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <sys/types.h>

typedef struct {
    int fd;
    char *request;
    size_t request_len;
    bool request_done;
    bool head_only;
    int file_fd;
    off_t file_remaining;
    char response[8192];
    size_t response_len;
    size_t response_sent;
    char *file_buffer;
    size_t file_buffer_len;
    size_t file_buffer_sent;
} client_t;

void client_reset(client_t *client);
void client_close(client_t *client);
bool client_send_pending(client_t *client);

#endif
