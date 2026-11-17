#include "server.h"

int chat_serv_sock_fd; //server socket

/////////////////////////////////////////////
// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE

int numReaders = 0; // keep count of the number of readers

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex lock
pthread_mutex_t rw_lock = PTHREAD_MUTEX_INITIALIZER;  // read/write lock

/////////////////////////////////////////////


char const *server_MOTD = "Thanks for connecting to the BisonChat Server.\n\nchat>";

struct node *head = NULL;          // list of all connected users
struct room *rooms_head = NULL;    // list of all rooms

/////////////////////////////////////////////
// Readers-Writers (readers preference, as in the textbook):
//  - many readers may hold the data at once
//  - a writer needs exclusive access (no readers, no other writers)
//  - `mutex` protects numReaders; `rw_lock` is held by the writer,
//    or by the group of readers as a whole (first in locks, last out unlocks)

void reader_lock(void) {
   pthread_mutex_lock(&mutex);
   numReaders++;
   if(numReaders == 1)
      pthread_mutex_lock(&rw_lock);     // first reader locks out writers
   pthread_mutex_unlock(&mutex);
}

void reader_unlock(void) {
   pthread_mutex_lock(&mutex);
   numReaders--;
   if(numReaders == 0)
      pthread_mutex_unlock(&rw_lock);   // last reader lets writers in
   pthread_mutex_unlock(&mutex);
}

void writer_lock(void) {
   pthread_mutex_lock(&rw_lock);
}

void writer_unlock(void) {
   pthread_mutex_unlock(&rw_lock);
}
/////////////////////////////////////////////


int main(int argc, char **argv) {
   (void)argc; (void)argv;

   signal(SIGINT, sigintHandler);
   signal(SIGPIPE, SIG_IGN);   // a client vanishing mid-send must not kill the server

   //////////////////////////////////////////////////////
   // create the default room for all clients to join when
   // initially connecting
   //////////////////////////////////////////////////////
   writer_lock();
   rooms_head = insertRoom(rooms_head, DEFAULT_ROOM);
   writer_unlock();

   // Open server socket
   chat_serv_sock_fd = get_server_socket();

   // step 3: get ready to accept connections
   if(start_server(chat_serv_sock_fd, BACKLOG) == -1) {
      printf("start server error\n");
      exit(1);
   }

   printf("Server Launched! Listening on PORT: %d\n", PORT);

   // Client threads block SIGINT, so Ctrl-C is always handled by this
   // main thread, which never holds a lock when the signal arrives.
   sigset_t sigint_set;
   sigemptyset(&sigint_set);
   sigaddset(&sigint_set, SIGINT);

   //Main execution loop
   while(1) {
      //Accept a connection, start a thread
      int new_client = accept_client(chat_serv_sock_fd);
      if(new_client != -1) {
         // give each thread its own copy of the socket number
         int *client_ptr = malloc(sizeof(int));
         if(client_ptr == NULL) {
            close(new_client);
            continue;
         }
         *client_ptr = new_client;

         pthread_t new_client_thread;
         pthread_sigmask(SIG_BLOCK, &sigint_set, NULL);     // new thread inherits the block
         if(pthread_create(&new_client_thread, NULL, client_receive, (void *)client_ptr) != 0) {
            close(new_client);
            free(client_ptr);
         } else {
            pthread_detach(new_client_thread);              // nobody joins client threads
         }
         pthread_sigmask(SIG_UNBLOCK, &sigint_set, NULL);
      }
   }

   close(chat_serv_sock_fd);
}


int get_server_socket(void) {
    int opt = TRUE;
    int master_socket;
    struct sockaddr_in address;

    //create a master socket
    if( (master_socket = socket(AF_INET , SOCK_STREAM , 0)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    //set master socket to allow multiple connections ,
    //this is just a good habit, it will work without this
    if( setsockopt(master_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt,
          sizeof(opt)) < 0 )
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    //type of socket created
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    //bind the socket to localhost port 8888
    if (bind(master_socket, (struct sockaddr *)&address, sizeof(address))<0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

   return master_socket;
}


int start_server(int serv_socket, int backlog) {
   int status = 0;
   if ((status = listen(serv_socket, backlog)) == -1) {
      printf("socket listen error\n");
   }
   return status;
}


int accept_client(int serv_sock) {
   int reply_sock_fd = -1;
   socklen_t sin_size = sizeof(struct sockaddr_storage);
   struct sockaddr_storage client_addr;
   //char client_printable_addr[INET6_ADDRSTRLEN];

   // accept a connection request from a client
   // the returned file descriptor from accept will be used
   // to communicate with this client.
   if ((reply_sock_fd = accept(serv_sock,(struct sockaddr *)&client_addr, &sin_size)) == -1) {
      printf("socket accept error\n");
   }
   return reply_sock_fd;
}


/* Handle SIGINT (CTRL+C) */
void sigintHandler(int sig_num) {
   (void)sig_num;
   char msg[128];

   printf("Error:Forced Exit.\n");

   //////////////////////////////////////////////////////////
   //Closing client sockets and freeing memory from user list
   //////////////////////////////////////////////////////////

   // Take exclusive (writer) access so no client thread is using the
   // lists while we tear them down.
   writer_lock();

   // Warn everyone, give them a moment, then shut down.
   snprintf(msg, sizeof(msg),
            "\n::server> The server is going down in %d seconds. Goodbye!\n",
            SHUTDOWN_DELAY);
   for(struct node *u = head; u != NULL; u = u->next)
      send(u->socket, msg, strlen(msg), MSG_NOSIGNAL);
   printf("Warned users. Shutting down in %d seconds...\n", SHUTDOWN_DELAY);
   sleep(SHUTDOWN_DELAY);

   printf("--------CLOSING ACTIVE USERS--------\n");

   for(struct node *u = head; u != NULL; u = u->next) {
      printf("Closing %s (socket %d)\n", u->username, u->socket);
      shutdown(u->socket, SHUT_RDWR);
      close(u->socket);
   }

   freeUsers(head);
   head = NULL;
   freeRooms(rooms_head);
   rooms_head = NULL;

   close(chat_serv_sock_fd);
   printf("All users disconnected, memory freed. Server stopped.\n");

   // Client threads are still blocked waiting for the lock; exit() ends them.
   exit(0);
}