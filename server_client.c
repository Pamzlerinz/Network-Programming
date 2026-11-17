#include "server.h"

#define DEFAULT_ROOM "Lobby"

// USE THESE LOCKS AND COUNTER TO SYNCHRONIZE
extern int numReaders;
extern pthread_mutex_t rw_lock;
extern pthread_mutex_t mutex;

extern struct node *head;
extern struct room *rooms_head;

extern char const *server_MOTD;


/*
 *Main thread for each client.  Receives all messages
 *and passes the data off to the correct function.  Receives
 *a pointer to the file descriptor for the socket the thread
 *should listen on
 */

char *trimwhitespace(char *str)
{
  char *end;

  // Trim leading space
  while(isspace((unsigned char)*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace((unsigned char)*end)) end--;

  // Write new null terminator character
  end[1] = '\0';

  return str;
}

// send a string to one client (never raises SIGPIPE)
static void send_str(int sock, const char *str) {
   send(sock, str, strlen(str), MSG_NOSIGNAL);
}

// Remove a user from every room and direct connection, then from the
// user list. Caller must NOT hold the lock.
static void remove_user(int client) {
   writer_lock();
   removeFromAllRooms(rooms_head, client);
   head = removeU(head, client);     // also removes them from everyone's DMs
   writer_unlock();
}

void *client_receive(void *ptr) {
   int client = *(int *) ptr;  // socket
   free(ptr);                  // main() malloc'd this for us

   int received, i;
   char buffer[MAXBUFF], sbuffer[MAXBUFF];  //data buffer of 2K
   char tmpbuf[MAXBUFF + 64];  //data temp buffer
   char cmd[MAXBUFF], username[NAME_LEN];
   char *arguments[80];
   char *message;

   struct node *currentUser;
   struct node *me;
   struct node *target;
   struct room *room;

   send_str(client, server_MOTD); // Send Welcome Message of the Day.

   // Creating the guest user name
   snprintf(username, sizeof(username), "guest%d", client);

   writer_lock();
   head = insertFirstU(head, client , username);

   // Add the GUEST to the DEFAULT ROOM (i.e. Lobby)
   room = findRoom(rooms_head, DEFAULT_ROOM);
   if(room != NULL)
      room->members = addMember(room->members, client);
   writer_unlock();

   printf("%s connected and joined %s\n", username, DEFAULT_ROOM);

   while (1) {

      received = read(client, buffer, MAXBUFF - 1);

      // Client closed the connection (or an error): clean up like "exit".
      if (received <= 0) {
         printf("socket %d disconnected\n", client);
         remove_user(client);
         close(client);
         return NULL;
      }

      buffer[received] = '\0';

      // The chat message (for non-commands) is the whole line, trimmed.
      strcpy(sbuffer, buffer);
      message = trimwhitespace(sbuffer);

      if (*message == '\0') {            // blank line: just re-prompt
         send_str(client, "chat>");
         continue;
      }

      strcpy(cmd, buffer);

      /////////////////////////////////////////////////////
      // we got some data from a client

      // 1. Tokenize the input in buf (split it on whitespace)

      // get the first token
      arguments[0] = strtok(cmd, delimiters);

      // walk through other tokens
      i = 0;
      while( arguments[i] != NULL && i < 79 ) {
         arguments[++i] = strtok(NULL, delimiters);
      }
      arguments[79] = NULL;

      // Arg[0] = command
      // Arg[1] = user or room

      /////////////////////////////////////////////////////
      // 2. Execute command

      // Commands that need an argument
      if ((strcmp(arguments[0], "create") == 0 || strcmp(arguments[0], "join") == 0 ||
           strcmp(arguments[0], "leave") == 0 || strcmp(arguments[0], "connect") == 0 ||
           strcmp(arguments[0], "disconnect") == 0 || strcmp(arguments[0], "login") == 0)
          && arguments[1] == NULL)
      {
         snprintf(buffer, sizeof(buffer), "Usage: %s <%s>\nchat>", arguments[0],
                  (strcmp(arguments[0], "connect") == 0 || strcmp(arguments[0], "disconnect") == 0)
                     ? "user" : (strcmp(arguments[0], "login") == 0 ? "username" : "room"));
         send_str(client, buffer);
         continue;
      }

      // Names must fit in our structs
      if (arguments[1] != NULL && strlen(arguments[1]) >= NAME_LEN &&
          strcmp(arguments[0], "users") != 0 && strcmp(arguments[0], "rooms") != 0)
      {
         snprintf(buffer, sizeof(buffer), "Error: name must be under %d characters.\nchat>", NAME_LEN);
         send_str(client, buffer);
         continue;
      }

      if(strcmp(arguments[0], "create") == 0)
      {
         printf("create room: %s\n", arguments[1]);

         // perform the operations to create room arg[1]
         writer_lock();
         if (findRoom(rooms_head, arguments[1]) != NULL) {
            snprintf(buffer, sizeof(buffer), "Room '%s' already exists.\n", arguments[1]);
         } else {
            rooms_head = insertRoom(rooms_head, arguments[1]);
            snprintf(buffer, sizeof(buffer), "Room '%s' created.\n", arguments[1]);
         }
         writer_unlock();

         strcat(buffer, "chat>");
         send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "join") == 0)
      {
         printf("join room: %s\n", arguments[1]);

         // perform the operations to join room arg[1]
         writer_lock();
         room = findRoom(rooms_head, arguments[1]);
         if (room == NULL) {
            snprintf(buffer, sizeof(buffer), "Room '%s' does not exist.\n", arguments[1]);
         } else if (hasMember(room->members, client)) {
            snprintf(buffer, sizeof(buffer), "You are already in room '%s'.\n", arguments[1]);
         } else {
            room->members = addMember(room->members, client);
            snprintf(buffer, sizeof(buffer), "Joined room '%s'.\n", arguments[1]);
         }
         writer_unlock();

         strcat(buffer, "chat>");
         send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "leave") == 0)
      {
         printf("leave room: %s\n", arguments[1]);

         // perform the operations to leave room arg[1]
         writer_lock();
         room = findRoom(rooms_head, arguments[1]);
         if (room == NULL) {
            snprintf(buffer, sizeof(buffer), "Room '%s' does not exist.\n", arguments[1]);
         } else if (!hasMember(room->members, client)) {
            snprintf(buffer, sizeof(buffer), "You are not in room '%s'.\n", arguments[1]);
         } else {
            room->members = removeMember(room->members, client);
            snprintf(buffer, sizeof(buffer), "Left room '%s'.\n", arguments[1]);
         }
         writer_unlock();

         strcat(buffer, "chat>");
         send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "connect") == 0)
      {
         printf("connect to user: %s \n", arguments[1]);

         // perform the operations to connect user with socket = client from arg[1]
         // (the connection goes both ways, so either user can DM the other)
         writer_lock();
         me = findU_by_socket(head, client);
         target = findU(head, arguments[1]);
         if (target == NULL) {
            snprintf(buffer, sizeof(buffer), "User '%s' not found.\n", arguments[1]);
         } else if (target->socket == client) {
            snprintf(buffer, sizeof(buffer), "You can't connect to yourself.\n");
         } else if (me != NULL && hasMember(me->dms, target->socket)) {
            snprintf(buffer, sizeof(buffer), "You are already connected to '%s'.\n", arguments[1]);
         } else if (me != NULL) {
            me->dms = addMember(me->dms, target->socket);
            target->dms = addMember(target->dms, client);
            snprintf(buffer, sizeof(buffer), "Connected to '%s'.\n", arguments[1]);
            snprintf(tmpbuf, sizeof(tmpbuf), "\n::server> %s is now directly connected to you.\nchat>", me->username);
            send_str(target->socket, tmpbuf);
         }
         writer_unlock();

         strcat(buffer, "chat>");
         send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "disconnect") == 0)
      {
         printf("disconnect from user: %s\n", arguments[1]);

         // perform the operations to disconnect user with socket = client from arg[1]
         writer_lock();
         me = findU_by_socket(head, client);
         target = findU(head, arguments[1]);
         if (target == NULL) {
            snprintf(buffer, sizeof(buffer), "User '%s' not found.\n", arguments[1]);
         } else if (me == NULL || !hasMember(me->dms, target->socket)) {
            snprintf(buffer, sizeof(buffer), "You are not connected to '%s'.\n", arguments[1]);
         } else {
            me->dms = removeMember(me->dms, target->socket);
            target->dms = removeMember(target->dms, client);
            snprintf(buffer, sizeof(buffer), "Disconnected from '%s'.\n", arguments[1]);
         }
         writer_unlock();

         strcat(buffer, "chat>");
         send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "rooms") == 0)
      {
          printf("List all the rooms\n");

          // put the list of rooms into buffer to send to client
          // (* marks rooms you are in)
          strcpy(buffer, "Rooms:\n");
          reader_lock();
          for (room = rooms_head; room != NULL; room = room->next) {
             int count = 0;
             for (struct member *m = room->members; m != NULL; m = m->next)
                count++;
             snprintf(tmpbuf, sizeof(tmpbuf), "  %s%s (%d user%s)\n",
                      room->name, hasMember(room->members, client) ? " *" : "",
                      count, count == 1 ? "" : "s");
             if (strlen(buffer) + strlen(tmpbuf) < MAXBUFF - 8)
                strcat(buffer, tmpbuf);
          }
          reader_unlock();

          strcat(buffer, "chat>");
          send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "users") == 0)
      {
          printf("List all the users\n");

          // put the list of users into buffer to send to client
          strcpy(buffer, "Users:\n");
          reader_lock();
          me = findU_by_socket(head, client);
          for (currentUser = head; currentUser != NULL; currentUser = currentUser->next) {
             const char *tag = "";
             if (currentUser->socket == client)
                tag = " (you)";
             else if (me != NULL && hasMember(me->dms, currentUser->socket))
                tag = " (DM)";
             snprintf(tmpbuf, sizeof(tmpbuf), "  %s%s\n", currentUser->username, tag);
             if (strlen(buffer) + strlen(tmpbuf) < MAXBUFF - 8)
                strcat(buffer, tmpbuf);
          }
          reader_unlock();

          strcat(buffer, "chat>");
          send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "login") == 0)
      {
          //rename their guestID to username. Rooms and DMs store sockets,
          //so they automatically show the updated username.
          writer_lock();
          me = findU_by_socket(head, client);
          target = findU(head, arguments[1]);
          if (target != NULL && target->socket != client) {
             snprintf(buffer, sizeof(buffer), "Username '%s' is already taken.\n", arguments[1]);
          } else if (me != NULL) {
             printf("%s logged in as %s\n", me->username, arguments[1]);
             strncpy(me->username, arguments[1], NAME_LEN - 1);
             me->username[NAME_LEN - 1] = '\0';
             snprintf(buffer, sizeof(buffer), "Logged in as '%s'.\n", me->username);
          }
          writer_unlock();

          strcat(buffer, "chat>");
          send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "help") == 0 )
      {
          snprintf(buffer, sizeof(buffer),
                   "login <username> - \"login with username\" \n"
                   "create <room> - \"create a room\" \n"
                   "join <room> - \"join a room\" \n"
                   "leave <room> - \"leave a room\" \n"
                   "users - \"list all users\" \n"
                   "rooms -  \"list all rooms\" \n"
                   "connect <user> - \"connect to user\" \n"
                   "disconnect <user> - \"disconnect from user\" \n"
                   "exit or logout - \"exit chat\" \n"
                   "chat>");
          send_str(client, buffer); // send back to client
      }
      else if (strcmp(arguments[0], "exit") == 0 || strcmp(arguments[0], "logout") == 0)
      {
          //Remove the initiating user from all rooms and direct connections, then close the socket descriptor.
          printf("socket %d exited\n", client);
          send_str(client, "Goodbye!\n");
          remove_user(client);
          close(client);
          return NULL;
      }
      else {
           /////////////////////////////////////////////////////////////
           // 3. sending a message

           // send a message in the following format followed by the prompt chat> to the
           // appropriate recipients: everyone who shares a room with the sender, plus
           // everyone directly connected (DM) to the sender. Each person gets it once.
           // ::[userfrom]> <message>

           reader_lock();
           me = findU_by_socket(head, client);
           if (me != NULL) {
              snprintf(tmpbuf, sizeof(tmpbuf), "\n::%s> %s\nchat>", me->username, message);

              for (currentUser = head; currentUser != NULL; currentUser = currentUser->next) {
                  if (currentUser->socket == client)   // dont send to yourself
                     continue;
                  if (shareRoom(rooms_head, client, currentUser->socket) ||
                      hasMember(me->dms, currentUser->socket)) {
                     send_str(currentUser->socket, tmpbuf);
                  }
              }
           }
           reader_unlock();

           send_str(client, "chat>");   // re-prompt the sender
      }

      memset(buffer, 0, sizeof(buffer));
   }
   return NULL;
}