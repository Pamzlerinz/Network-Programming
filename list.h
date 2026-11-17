#ifndef LIST_H
#define LIST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define NAME_LEN 30

/*
 * Members of a room, and a user's direct connections (DMs), are stored
 * by SOCKET rather than by name. That way, when a user logs in with a new
 * name, every room and DM automatically shows the updated username.
 */
struct member {
   int socket;
   struct member *next;
};

/////////////////// USERLIST //////////////////////////

struct node {
   char username[NAME_LEN];
   int socket;
   struct member *dms;      // sockets of users this user is directly connected to
   struct node *next;
};

//insert node at the first location
struct node* insertFirstU(struct node *head, int socket, char *username);

//find a node with given username
struct node* findU(struct node *head, char* username);

//find a node with given socket
struct node* findU_by_socket(struct node *head, int socket);

//remove the node with given socket (frees it and its DM list)
struct node* removeU(struct node *head, int socket);

//free every user node
void freeUsers(struct node *head);

/////////////////// MEMBER LIST (room members / DMs) //////////////////////////

//add socket to list (no duplicates); returns new head
struct member* addMember(struct member *head, int socket);

//remove socket from list; returns new head
struct member* removeMember(struct member *head, int socket);

//is socket in list?
bool hasMember(struct member *head, int socket);

//free the whole list
void freeMembers(struct member *head);

/////////////////// ROOMLIST //////////////////////////

struct room {
   char name[NAME_LEN];
   struct member *members;
   struct room *next;
};

//insert a room at the first location (no duplicates)
struct room* insertRoom(struct room *head, char *name);

//find a room by name
struct room* findRoom(struct room *head, char *name);

//remove a socket from every room
void removeFromAllRooms(struct room *head, int socket);

//do two users share at least one room?
bool shareRoom(struct room *head, int socketA, int socketB);

//free every room and its member list
void freeRooms(struct room *head);

#endif