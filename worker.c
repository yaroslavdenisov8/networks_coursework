#define _DARWIN_C_SOURCE
#include "worker.h"
#include "client.h"
#include "config.h"
#include "http.h"
#include "logger.h"
#include "net.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

extern volatile sig_atomic_t stop_server;

static void accept_ready_clients(int listen_fd, client_t *clients) {
    struct sockaddr_in address;
    socklen_t length = sizeof(address);
    int client_fd = accept(listen_fd, (struct sockaddr *)&address, &length);
    if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            log_event("accept failed: %s", strerror(errno));
        return;
    }
    if (client_fd >= FD_SETSIZE || set_nonblocking(client_fd) < 0) {
        close(client_fd);
        return;
    }
    client_t *client = &clients[client_fd];
    client_reset(client);
    client->fd = client_fd;
    client->request = calloc(MAX_REQUEST, 1);
    client->file_buffer = malloc(FILE_BUFFER);
    if (!client->request || !client->file_buffer) {
        client_close(client);
        return;
    }
    log_event("accepted client fd=%d", client_fd);
}

void worker_loop(int listen_fd, const char *root) {
    client_t *clients = calloc(FD_SETSIZE, sizeof(*clients));
    if (!clients) {
        log_event("worker failed to allocate client table");
        return;
    }
    for (int i = 0; i < FD_SETSIZE; ++i)
        client_reset(&clients[i]);

    while (!stop_server) {
        fd_set read_set, write_set;
        FD_ZERO(&read_set);
        FD_ZERO(&write_set);
        FD_SET(listen_fd, &read_set);
        int max_fd = listen_fd;
        for (int fd = 0; fd < FD_SETSIZE; ++fd) {
            if (clients[fd].fd < 0) continue;
            if (!clients[fd].request_done) FD_SET(fd, &read_set);
            else FD_SET(fd, &write_set);
            if (fd > max_fd) max_fd = fd;
        }

        int ready = select(max_fd + 1, &read_set, &write_set, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            log_event("select failed: %s", strerror(errno));
            break;
        }
        if (FD_ISSET(listen_fd, &read_set)) 
            accept_ready_clients(listen_fd, clients);

        for (int fd = 0; fd <= max_fd; ++fd) {
            client_t *client = &clients[fd];
            if (client->fd < 0) continue;
            if (FD_ISSET(fd, &read_set)) {
                ssize_t received = recv(fd, client->request + client->request_len,
                                        MAX_REQUEST - client->request_len - 1, 0);
                if (received == 0) {
                    log_event("client fd=%d closed connection before request", fd);
                    client_close(client);
                    continue;
                }
                if (received < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                        log_event("recv failed fd=%d: %s", fd, strerror(errno));
                    if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                        client_close(client);
                    continue;
                }
                client->request_len += (size_t)received;
                client->request[client->request_len] = '\0';
                log_event("received fd=%d bytes=%zd total=%zu", fd, received,
                          client->request_len);
                if (client->request_len == MAX_REQUEST - 1) {
                    client->response_len = (size_t)snprintf(
                        client->response, sizeof(client->response),
                        "HTTP/1.1 400 Bad Request\r\nContent-Length: 16\r\n"
                        "Connection: close\r\n\r\n400 Bad Request\n");
                    client->response_sent = 0;
                    client->request_done = true;
                } else if (find_header_end(client->request, client->request_len)) {
                    struct sockaddr_in peer = {0};
                    socklen_t peer_length = sizeof(peer);
                    getpeername(fd, (struct sockaddr *)&peer, &peer_length);
                    process_request(client, root, &peer);
                    client->request_done = true;
                }
            }
            if (client->fd >= 0 && client->request_done && FD_ISSET(fd, &write_set) &&
                !client_send_pending(client)) client_close(client);
        }
    }
    for (int fd = 0; fd < FD_SETSIZE; ++fd)
        if (clients[fd].fd >= 0) client_close(&clients[fd]);
    free(clients);
}
