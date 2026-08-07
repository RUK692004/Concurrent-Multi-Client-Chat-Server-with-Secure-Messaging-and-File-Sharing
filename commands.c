#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/socket.h>

#include "commands.h"
#include "rooms.h"
#include "utils.h"

void handle_command(Client *client, char *command)
{
    // Handle /exit command
    if (strcmp(command, "/exit") == 0)
    {
        printf(YELLOW "[INFO] %s requested disconnect.\n" RESET, client->username);

        char msg[100];
        sprintf(msg, "%s left the chat.", client->username);
        broadcast(msg, client->socket);

        remove_client(client->socket);
        close(client->socket);
        pthread_exit(NULL);
    }
    // Handle /msg command (private messaging)
    else if (strncmp(command, "/msg ", 5) == 0)
    {
        char recipient[USERNAME_SIZE];
        char private_msg[BUFFER_SIZE];

        char *space = strchr(command + 5, ' ');
        if (space == NULL)
        {
            send(client->socket,
                 "SYS|Usage: /msg <username> <message>\n",
                 strlen("SYS|Usage: /msg <username> <message>\n"),
                 0);
            return;
        }

        int name_len = space - (command + 5);
        strncpy(recipient, command + 5, name_len);
        recipient[name_len] = '\0';

        strcpy(private_msg, space + 1);

        // Find recipient
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i] != NULL &&
                strcmp(clients[i]->username, recipient) == 0)
            {
                // Send private message to recipient
                char msg[BUFFER_SIZE + 100];
                sprintf(msg, "PRV|%s|%s\n", client->username, private_msg);
                send(clients[i]->socket, msg, strlen(msg), 0);

                // Also send to sender for confirmation
                sprintf(msg, "PRV|To %s: %s\n", recipient, private_msg);
                send(client->socket, msg, strlen(msg), 0);

                printf(CYAN "[PRIVATE] %s -> %s: %s\n" RESET,
                       client->username, recipient, private_msg);
                return;
            }
        }

        // User not found
        char response[BUFFER_SIZE];
        sprintf(response, "SYS|User '%s' not found.\n", recipient);
        send(client->socket, response, strlen(response), 0);
    }
    // Handle /list command
    else if (strcmp(command, "/list") == 0)
    {
        list_rooms(client->socket);
    }
    // Handle /leave command
    else if (strcmp(command, "/leave") == 0)
    {
        // Move user back to Lobby
        strcpy(client->room, "Lobby");

        printf(YELLOW "%s left room to Lobby\n" RESET, client->username);

        send(client->socket,
             "SYS|You moved to Lobby.\n",
             strlen("SYS|You moved to Lobby.\n"),
             0);
    }
    // Handle /join command
    else if (strncmp(command, "/join ", 6) == 0)
    {
        char room_name[USERNAME_SIZE];
        strcpy(room_name, command + 6);

        // Remove trailing newline if present
        room_name[strcspn(room_name, "\n")] = '\0';

        // Find room
        if (room_exists(room_name))
        {
            // Join the room
            strcpy(client->room, room_name);

            printf(GREEN "%s joined room '%s'\n" RESET, client->username, room_name);

            char response[BUFFER_SIZE];
            sprintf(response, "SYS|You joined room '%s'.\n", room_name);
            send(client->socket, response, strlen(response), 0);
        }
        else
        {
            // Room not found
            char response[BUFFER_SIZE];
            sprintf(response, "SYS|Room '%s' does not exist.\n", room_name);
            send(client->socket, response, strlen(response), 0);
        }
    }
    // Handle /create command
    else if (strncmp(command, "/create ", 8) == 0)
    {
        char room_name[USERNAME_SIZE];
        strcpy(room_name, command + 8);

        // Remove trailing newline if present
        room_name[strcspn(room_name, "\n")] = '\0';

        int result = create_room(room_name);
        if (result == 1)
        {
            printf(GREEN "Room '%s' created by %s\n" RESET, room_name, client->username);

            char response[BUFFER_SIZE];
            sprintf(response, "SYS|Room '%s' created.\n", room_name);
            send(client->socket, response, strlen(response), 0);
        }
        else if (result == 0)
        {
            char response[BUFFER_SIZE];
            sprintf(response, "SYS|Room '%s' already exists.\n", room_name);
            send(client->socket, response, strlen(response), 0);
        }
        else
        {
            send(client->socket, "SYS|Maximum rooms reached.\n", 27, 0);
        }
    }
    else
    {
        char response[BUFFER_SIZE];
        sprintf(response, "SYS|Unknown command: %s\n", command);
        send(client->socket, response, strlen(response), 0);

        send(client->socket,
             "SYS|Available commands:\n",
             strlen("SYS|Available commands:\n"),
             0);

        send(client->socket,
             "SYS|  /create <room>\n",
             strlen("SYS|  /create <room>\n"),
             0);

        send(client->socket,
             "SYS|  /join <room>\n",
             strlen("SYS|  /join <room>\n"),
             0);

        send(client->socket,
             "SYS|  /leave\n",
             strlen("SYS|  /leave\n"),
             0);

        send(client->socket,
             "SYS|  /list\n",
             strlen("SYS|  /list\n"),
             0);

        send(client->socket,
             "SYS|  /msg <user> <message>\n",
             strlen("SYS|  /msg <user> <message>\n"),
             0);
    }
}