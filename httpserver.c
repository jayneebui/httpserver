
#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "httpserver.h"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

int main(int argc, char **argv) {
    int opt = 0;
    int threads = 4;

    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't':
            threads = atoi(optarg);
            if (threads <= 0) {
                errx(EXIT_FAILURE, "bad number of threads");
            }
            break;
        default: usage(argv[0]); return EXIT_FAILURE;
        }
    }

    if (optind >= argc) {
        warnx("wrong number of arguments");
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (port == 0) {
        errx(EXIT_FAILURE, "bad port number: %s", argv[1]);
    }

    //signal handlers
    signal(SIGPIPE, SIG_IGN);
    signal(SIGTERM, sig_handler);
    signal(SIGINT, sig_handler);

    // initializing global queue
    q = queue_new(threads);

    // initialize threadpool
    pthread_t threadpool[threads];
    for (int i = 0; i < threads; i++) {
        if (pthread_create(&threadpool[i], NULL, do_twork, NULL) != 0) {
            err(EXIT_FAILURE, "pthread_create() has failed.");
        }
    }

    // creating & initializing server socket
    Listener_Socket server_sock;
    listener_init(&server_sock, port);

    // dispatcher
    while (1) {
        uintptr_t client_fd = (uintptr_t) listener_accept(&server_sock);
        queue_push(q, (void *) client_fd);
    }

    pthread_mutex_destroy(&mutex);
    return EXIT_SUCCESS;
}

// worker thread function
void *do_twork() {
    while (1) {
        uintptr_t fd = 0;
        queue_pop(q, (void **) &fd); // pops connection from queue and hands it to worker
        if (fd > 0) {
            handle_connection(fd);
        }
        close(fd);
    }
}

// handles connection
void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);
    const Response_t *res = conn_parse(conn);
    const Request_t *req = conn_get_request(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }
    conn_delete(&conn);
}

void audit_logging(char *oper, char *uri, uint16_t status_code, char *req_id) {
    logfile = stderr;
    if (req_id == NULL) {
        req_id = "0";
    }

    fprintf(logfile, "%s,/%s,%hu,%s\n", oper, uri, status_code, req_id);
}

void handle_get(conn_t *conn) {
    const Response_t *res = NULL;
    char *uri = conn_get_uri(conn);

    pthread_mutex_lock(&mutex);
    bool existed = access(uri, F_OK) == 0;
    int fd = open(uri, O_RDONLY, 0644);
    if (res == NULL && !existed) {
        res = &RESPONSE_NOT_FOUND;
        uint16_t code = response_get_code(res);
        char *request_id = conn_get_header(conn, "Request-Id");
        pthread_mutex_unlock(&mutex);
        audit_logging("GET", uri, code, request_id);
        conn_send_response(conn, res);
        return;
    }

    flock(fd, LOCK_SH);
    pthread_mutex_unlock(&mutex);

    if (fd < 0) {
        if (errno == EACCES) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else if (errno == ENOENT) {
            res = &RESPONSE_NOT_FOUND;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }

    struct stat stats;
    fstat(fd, &stats);

    if (S_ISDIR(stats.st_mode)) {
        res = &RESPONSE_FORBIDDEN;
        goto out;
    }

    res = conn_send_file(conn, fd, stats.st_size);
    uint16_t code = response_get_code(&RESPONSE_OK);
    char *request_id = conn_get_header(conn, "Request-Id");
    audit_logging("GET", uri, code, request_id);
    close(fd);
    return;
out:
    code = response_get_code(res);
    request_id = conn_get_header(conn, "Request-Id");
    audit_logging("GET", uri, code, request_id);
    close(fd);
    conn_send_response(conn, res);
}

void handle_unsupported(conn_t *conn) {
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
}

void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;
    uint16_t code = 0;

    pthread_mutex_lock(&mutex);

    bool existed = access(uri, F_OK) == 0;

    int fd = open(uri, O_CREAT | O_WRONLY, 0600);

    flock(fd, LOCK_EX);
    ftruncate(fd, 0);
    pthread_mutex_unlock(&mutex);
    if (fd < 0) {
        // debug("%s: %d", uri, errno);
        if (errno == EACCES || errno == EISDIR || errno == ENOENT) {
            res = &RESPONSE_FORBIDDEN;
            goto out;
        } else {
            res = &RESPONSE_INTERNAL_SERVER_ERROR;
            goto out;
        }
    }
    // writing data
    res = conn_recv_file(conn, fd);

    if (res == NULL && existed) {
        res = &RESPONSE_OK;
    } else if (res == NULL && !existed) {
        res = &RESPONSE_CREATED;
    }

out:
    code = response_get_code(res);
    char *request_id = conn_get_header(conn, "Request-Id");
    audit_logging("PUT", uri, code, request_id);
    close(fd);
    conn_send_response(conn, res);
}

static void sig_handler(int sig) {
    if (sig == SIGINT) {
        warnx("received SIGIGN");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    } else if (sig == SIGTERM) {
        warnx("received SIGTERM");
        fclose(logfile);
        exit(EXIT_SUCCESS);
    }
}

static void usage(char *exec) {
    fprintf(stderr, "usage: %s [-t threads] <port>\n", exec);
}
