#define _DARWIN_C_SOURCE
#include "config.h"
#include "logger.h"
#include "net.h"
#include "worker.h"
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t stop_server = 0;

static void handle_signal(int signal_number) {
    (void)signal_number;
    stop_server = 1;
}

static int parse_positive(const char *value, int minimum, int maximum) {
    char *end = NULL;
    long parsed = strtol(value, &end, 10);
    if (*value == '\0' || *end != '\0' || parsed < minimum || parsed > maximum)
        return -1;
    return (int)parsed;
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <directory> [port] [workers]\n", argv[0]);
        return EXIT_FAILURE;
    }
    char root[PATH_MAX];
    if (!realpath(argv[1], root)) { perror("realpath"); return EXIT_FAILURE; }
    struct stat root_stat;
    if (stat(root, &root_stat) < 0 || !S_ISDIR(root_stat.st_mode)) {
        fprintf(stderr, "Not a directory: %s\n", root);
        return EXIT_FAILURE;
    }

    int port = argc >= 3 ? parse_positive(argv[2], 1, 65535) : DEFAULT_PORT;
    int workers = argc >= 4 ? parse_positive(argv[3], 1, 128) : DEFAULT_WORKERS;
    if (port < 0 || workers < 0) {
        fprintf(stderr, "Invalid port or worker count\n");
        return EXIT_FAILURE;
    }
    if (logger_open("server.log") < 0) { 
        perror("server.log"); 
        return EXIT_FAILURE; 
    }

    struct sigaction action = {0};
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);

    int listen_fd = create_listener(port);
    if (listen_fd < 0) {
        perror("listen");
        logger_close();
        return EXIT_FAILURE;
    }
    pid_t *children = calloc((size_t)workers, sizeof(*children));
    if (!children) {
        perror("calloc");
        close(listen_fd);
        logger_close();
        return EXIT_FAILURE;
    }

    log_event("server started root=%s port=%d workers=%d", root, port, workers);
    int created = 0;
    for (; created < workers; ++created) {
        pid_t child = fork();
        if (child < 0) { 
            perror("fork"); 
            stop_server = 1; 
            break; 
        }
        if (child == 0) {
            worker_loop(listen_fd, root);
            close(listen_fd);
            logger_close();
            exit(EXIT_SUCCESS);
        }
        children[created] = child;
    }

    while (!stop_server) {
        int status;
        pid_t result = waitpid(-1, &status, 0);
        if (result < 0 && errno == EINTR) 
            continue;
        if (result > 0) { 
            log_event("worker pid=%ld exited", (long)result); 
            break; 
        }
        if (result < 0)
            break;
    }
    stop_server = 1;
    for (int i = 0; i < created; ++i)
        if (children[i] > 0) kill(children[i], SIGTERM);
    for (int i = 0; i < created; ++i)
        if (children[i] > 0) waitpid(children[i], NULL, 0);

    log_event("server stopped");
    free(children);
    close(listen_fd);
    logger_close();

    return EXIT_SUCCESS;
}
