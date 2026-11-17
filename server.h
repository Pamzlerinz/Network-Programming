#ifndef SERVER_H
#define SERVER_H

/* System Header Files */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/wait.h>
#include <netdb.h>
#include <ctype.h>
#include <pthread.h>

/* Local Header Files */
#include "list.h"

#define MAX_READERS 25
#define TRUE   1
#define FALSE  0
#define PORT 8888
#define delimiters " \t\r\n"   // split commands on any whitespace (telnet sends \r\n)
#define max_clients  30
#define DEFAULT_ROOM "Lobby"
#define MAXBUFF   2096
#define BACKLOG 2
#define SHUTDOWN_DELAY 3       // seconds of warning users get before the server goes down


// shared data structures (defined in server.c)
extern struct node *head;          // all connected users
extern struct room *rooms_head;    // all rooms

// prototypes

int get_server_socket(void);
int start_server(int serv_socket, int backlog);
int accept_client(int serv_sock);
void sigintHandler(int sig_num);
void *client_receive(void *ptr);

// readers-writers synchronization (defined in server.c)
void reader_lock(void);
void reader_unlock(void);
void writer_lock(void);
void writer_unlock(void);

#endif