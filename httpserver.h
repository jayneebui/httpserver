#ifndef __HTTPSERVER_H__
#define __HTTPSERVER_H__

#include <sys/socket.h>
#include <pthread.h>
#include <stdio.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/file.h>

#include "queue.h"

// audit logging
static FILE *logfile;
#define LOG(...) fprintf(logfile, __VA_ARGS__);

// locks
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// globalize queue
queue_t *q;

// functions provided by Dr. Andrew Quinn's Spring 2022 asgn3 starter code
static void usage(char *exec);
static void sig_handler(int sig);

// working thread processes function
void *do_twork();

// audit logging function
void audit_logging(char *oper, char *uri, uint16_t status_code, char *req_id);

#endif
