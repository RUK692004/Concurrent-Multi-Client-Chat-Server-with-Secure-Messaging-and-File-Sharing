#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/socket.h>

#include "utils.h"

Client *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
Room rooms[MAX_ROOMS];
int room_count = 1;

void add_client(Client *client)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i] == NULL)
        {
            clients[i] = client;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int socket)
{
    pthread_mutex_lock(&clients_mutex);

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i] != NULL &&
           clients[i]->socket == socket)
        {
            free(clients[i]);
            clients[i] = NULL;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void broadcast(char *message, int sender_socket)
{
    pthread_mutex_lock(&clients_mutex);

    char sender_username[USERNAME_SIZE] = "Unknown";

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i] != NULL &&
           clients[i]->socket == sender_socket)
        {
            strcpy(sender_username, clients[i]->username);
            break;
        }
    }

    char msg[BUFFER_SIZE + 100];

    // Check if this is a system message (join/leave)
    if (strstr(message, "joined the chat.") != NULL || 
        strstr(message, "left the chat.") != NULL)
    {
        sprintf(msg, "SYS|%s\n", message);
    }
    else
    {
        sprintf(msg, "MSG|%s|%s\n", sender_username, message);
    }

    printf("%s", msg);

    for(int i = 0; i < MAX_CLIENTS; i++)
    {
        if(clients[i] != NULL &&
           clients[i]->socket != sender_socket)
        {
            send(clients[i]->socket,
                 msg,
                 strlen(msg),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_room_message(Client *client, char *buffer)
{
    pthread_mutex_lock(&clients_mutex);

    char sender_username[USERNAME_SIZE];
    strcpy(sender_username, client->username);

    char msg[BUFFER_SIZE + 100];

    if (strstr(buffer, "joined the chat.") != NULL || 
        strstr(buffer, "left the chat.") != NULL)
    {
        sprintf(msg, "SYS|%s\n", buffer);
    }
    else
    {
        sprintf(msg, "MSG|%s|%s\n", sender_username, buffer);
    }

    printf("%s", msg);

    // Send only to clients in the same room
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (clients[i] != NULL &&
            clients[i]->socket != client->socket &&
            strcmp(clients[i]->room, client->room) == 0)
        {
            send(clients[i]->socket,
                 msg,
                 strlen(msg),
                 0);
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}