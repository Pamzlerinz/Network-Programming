#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "list.h"

/////////////////// USERLIST //////////////////////////

//insert link at the first location
struct node* insertFirstU(struct node *head, int socket, char *username) {

   if(findU(head,username) == NULL) {

       //create a link
       struct node *link = (struct node*) malloc(sizeof(struct node));
       if(link == NULL) {
           perror("malloc");
           return head;
       }

       link->socket = socket;
       strncpy(link->username, username, NAME_LEN - 1);
       link->username[NAME_LEN - 1] = '\0';
       link->dms = NULL;

       //point it to old first node
       link->next = head;

       //point first to new first node
       head = link;

   }
   else
       printf("Duplicate: %s\n", username);
   return head;
}

//find a link with given user
struct node* findU(struct node *head, char* username) {

   //start from the first link
   struct node* current = head;

   //if list is empty
   if(head == NULL) {
      return NULL;
   }

   //navigate through list
    while(strcmp(current->username, username) != 0) {

      //if it is last node
      if(current->next == NULL) {
         return NULL;
      } else {
         //go to next link
         current = current->next;
      }
   }

   //if username found, return the current Link
   return current;
}

//find a link with given socket
struct node* findU_by_socket(struct node *head, int socket) {
   struct node *current = head;
   while(current != NULL) {
      if(current->socket == socket)
         return current;
      current = current->next;
   }
   return NULL;
}

//remove the link with given socket; also removes that socket from
//every other user's DM list so no one is left connected to a ghost
struct node* removeU(struct node *head, int socket) {
   struct node *current = head, *prev = NULL;

   while(current != NULL && current->socket != socket) {
      prev = current;
      current = current->next;
   }
   if(current == NULL)
      return head;            // not found

   if(prev == NULL)
      head = current->next;
   else
      prev->next = current->next;

   freeMembers(current->dms);
   free(current);

   for(struct node *u = head; u != NULL; u = u->next)
      u->dms = removeMember(u->dms, socket);

   return head;
}

//free every user node
void freeUsers(struct node *head) {
   while(head != NULL) {
      struct node *next = head->next;
      freeMembers(head->dms);
      free(head);
      head = next;
   }
}

/////////////////// MEMBER LIST //////////////////////////

struct member* addMember(struct member *head, int socket) {
   if(hasMember(head, socket))
      return head;
   struct member *m = (struct member*) malloc(sizeof(struct member));
   if(m == NULL) {
      perror("malloc");
      return head;
   }
   m->socket = socket;
   m->next = head;
   return m;
}

struct member* removeMember(struct member *head, int socket) {
   struct member *current = head, *prev = NULL;
   while(current != NULL) {
      if(current->socket == socket) {
         if(prev == NULL)
            head = current->next;
         else
            prev->next = current->next;
         free(current);
         return head;
      }
      prev = current;
      current = current->next;
   }
   return head;
}

bool hasMember(struct member *head, int socket) {
   for(struct member *m = head; m != NULL; m = m->next)
      if(m->socket == socket)
         return true;
   return false;
}

void freeMembers(struct member *head) {
   while(head != NULL) {
      struct member *next = head->next;
      free(head);
      head = next;
   }
}

/////////////////// ROOMLIST //////////////////////////

struct room* insertRoom(struct room *head, char *name) {
   if(findRoom(head, name) != NULL)
      return head;            // already exists

   struct room *r = (struct room*) malloc(sizeof(struct room));
   if(r == NULL) {
      perror("malloc");
      return head;
   }
   strncpy(r->name, name, NAME_LEN - 1);
   r->name[NAME_LEN - 1] = '\0';
   r->members = NULL;
   r->next = head;
   return r;
}

struct room* findRoom(struct room *head, char *name) {
   for(struct room *r = head; r != NULL; r = r->next)
      if(strcmp(r->name, name) == 0)
         return r;
   return NULL;
}

void removeFromAllRooms(struct room *head, int socket) {
   for(struct room *r = head; r != NULL; r = r->next)
      r->members = removeMember(r->members, socket);
}

bool shareRoom(struct room *head, int socketA, int socketB) {
   for(struct room *r = head; r != NULL; r = r->next)
      if(hasMember(r->members, socketA) && hasMember(r->members, socketB))
         return true;
   return false;
}

void freeRooms(struct room *head) {
   while(head != NULL) {
      struct room *next = head->next;
      freeMembers(head->members);
      free(head);
      head = next;
   }
}