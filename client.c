#define _DARWIN_C_SOURCE
#include "client.h"
#include "config.h"
#include <errno.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

void client_reset(client_t *client) {
    client->fd = -1;
    client->request = NULL;
    client->request_len = 0;
    client->request_done = false;
    client->head_only = false;
    client->file_fd = -1;
    client->file_remaining = 0;
    client->response_len = 0;
    client->response_sent = 0;
    client->file_buffer = NULL;
    client->file_buffer_len = 0;
    client->file_buffer_sent = 0;
}

void client_close(client_t *client) {
    if (client->fd >= 0) close(client->fd);
    if (client->file_fd >= 0) close(client->file_fd);
    free(client->request);
    free(client->file_buffer);
    client_reset(client);
}

bool client_send_pending(client_t *client) {
    while (client->response_sent < client->response_len) {
        ssize_t sent = send(client->fd, client->response + client->response_sent,
                            client->response_len - client->response_sent, 0);
        if (sent > 0)
            client->response_sent += (size_t)sent;
        else if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
            return true; 
        else 
            return false;
    }
    if (client->head_only || client->file_fd < 0 || client->file_remaining == 0) 
        return false;

    while (client->file_remaining > 0 || client->file_buffer_sent < client->file_buffer_len) {
        if (client->file_buffer_sent == client->file_buffer_len) {
            ssize_t count = read(client->file_fd, client->file_buffer, FILE_BUFFER);
            if (count < 0 && errno == EINTR) 
                continue;
            if (count <= 0) 
                return false;
            client->file_buffer_len = (size_t)count;
            client->file_buffer_sent = 0;
        }
        ssize_t sent = send(client->fd, client->file_buffer + client->file_buffer_sent,
                            client->file_buffer_len - client->file_buffer_sent, 0);
        if (sent > 0) {
            client->file_buffer_sent += (size_t)sent;
            client->file_remaining -= sent;
        } else if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) 
            return true;
        else 
            return false;
    }
    return false;
}
