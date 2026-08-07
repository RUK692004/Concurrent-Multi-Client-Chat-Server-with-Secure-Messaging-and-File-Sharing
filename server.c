#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "utils.h"
#include "auth.h"
#include "rooms.h"
#include "commands.h"

void *client_handler(void *arg)
{
    Client *client = (Client *)arg;
    int client_socket = client->socket;

    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes = recv(client_socket, buffer, BUFFER_SIZE, 0);

        if (bytes <= 0)
        {
            printf(RED "[INFO] %s disconnected.\n" RESET, client->username);
            break;
        }

        buffer[bytes] = '\0';

        // Check for exit command
        if (strcmp(buffer, "exit") == 0)
        {
            handle_command(client, "/exit");
            continue;
        }

        // Check if command (starts with /)
        if (buffer[0] == '/')
        {
            handle_command(client, buffer);
        }
        else
        {
            broadcast_room_message(client, buffer);
        }
    }

    // Broadcast leave notification
    char leave_msg[100];
    sprintf(leave_msg, "%s left the chat.", client->username);
    broadcast(leave_msg, client_socket);

    remove_client(client_socket);

    close(client_socket);

    pthread_exit(NULL);
}

int main()
{
    int server_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t addr_size;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    printf(GREEN "Socket created successfully.\n" RESET);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf(GREEN "Bind successful.\n" RESET);

    if (listen(server_fd, 5) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf(GREEN "Server listening on port %d...\n" RESET, PORT);

    // Initialize rooms
    init_rooms();

    while (1)
    {
        addr_size = sizeof(client_addr);

        Client *client = malloc(sizeof(Client));

        if (client == NULL)
        {
            perror("Memory allocation failed");
            continue;
        }

        client->socket = accept(server_fd,
                        (struct sockaddr *)&client_addr,
                        &addr_size);

        if (client->socket < 0)
        {
            perror("Accept failed");
            free(client);
            continue;
        }

        // Receive login message (LOGIN|username|password)
        char login_buffer[BUFFER_SIZE];
        int login_bytes = recv(client->socket, login_buffer, BUFFER_SIZE - 1, 0);
        if (login_bytes <= 0)
        {
            printf("Failed to receive login.\n");
            free(client);
            continue;
        }
        login_buffer[login_bytes] = '\0';

        // Parse LOGIN|username|password
        char *token;
        
        token = strtok(login_buffer, "|");      // LOGIN
        if (token == NULL || strcmp(token, "LOGIN") != 0)
        {
            printf("Invalid login format.\n");
            close(client->socket);
            free(client);
            continue;
        }

        token = strtok(NULL, "|");              // username
        if (token == NULL)
        {
            printf("Invalid login format.\n");
            close(client->socket);
            free(client);
            continue;
        }

        char username[USERNAME_SIZE];
        strcpy(username, token);

        token = strtok(NULL, "|");              // password
        if (token == NULL)
        {
            printf("Invalid login format.\n");
            close(client->socket);
            free(client);
            continue;
        }

        char password[PASSWORD_SIZE];
        strcpy(password, token);

        // Remove newline from password if present
        password[strcspn(password, "\n")] = '\0';

        // Copy to client struct
        strcpy(client->username, username);

        // Authenticate user
        if (authenticate(client->username, password))
        {
            printf(GREEN "Authentication successful for %s\n" RESET, client->username);
            send(client->socket, "LOGIN_OK", 8, 0);

            // Initialize client room and authentication
            strcpy(client->room, "Lobby");
            client->authenticated = 1;

            add_client(client);

            // Send welcome message to new client
            send(client->socket,
                 "SYS|Welcome to the chat!\n",
                 strlen("SYS|Welcome to the chat!\n"),
                 0);

            // Broadcast join notification
            char join_msg[100];
            sprintf(join_msg, "%s joined the chat.", client->username);
            broadcast(join_msg, client->socket);
        }
        else
        {
            printf(RED "Authentication failed for %s\n" RESET, client->username);
            send(client->socket, "LOGIN_FAIL", 10, 0);
            close(client->socket);
            free(client);
            continue;
        }

        pthread_t thread_id;

        if (pthread_create(&thread_id,
                           NULL,
                           client_handler,
                           client) != 0)
        {
            perror("Thread creation failed");
            close(client->socket);
            free(client);
            continue;
        }

        pthread_detach(thread_id);
    }

    close(server_fd);

    return 0;
}