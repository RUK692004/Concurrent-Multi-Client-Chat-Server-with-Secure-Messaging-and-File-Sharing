#ifndef UTILS_H
#define UTILS_H

#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 32
#define PASSWORD_SIZE 32

// ANSI Color Codes
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN    "\033[36m"

#define MAX_CLIENTS 100
#define MAX_ROOMS 100

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    char room[USERNAME_SIZE];
    int authenticated;
    pthread_t thread;
} Client;

typedef struct
{
    char name[USERNAME_SIZE];
} Room;

extern Client *clients[MAX_CLIENTS];
extern pthread_mutex_t clients_mutex;
extern Room rooms[MAX_ROOMS];
extern int room_count;

void add_client(Client *client);
void remove_client(int socket);
void broadcast(char *message, int sender_socket);
void broadcast_room_message(Client *client, char *buffer);

#endif