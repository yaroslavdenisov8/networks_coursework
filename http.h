#ifndef HTTP_H
#define HTTP_H

#include "client.h"
#include <netinet/in.h>
#include <stdbool.h>

bool find_header_end(const char *buffer, size_t length);
void process_request(client_t *client, const char *root,
                     const struct sockaddr_in *address);

#endif
