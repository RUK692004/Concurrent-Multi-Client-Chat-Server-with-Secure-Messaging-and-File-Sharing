#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define USERNAME_SIZE 32

#define MAX_CLIENTS 100

typedef struct
{
    int socket;
    char username[USERNAME_SIZE];
    pthread_t thread;
} Client;

Client *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

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

    sprintf(msg,
            "%s > %s",
            sender_username,
            message);

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
            printf("Client disconnected.\n");
            break;
        }

        buffer[bytes] = '\0';

        broadcast(buffer, client_socket);
    }

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

    printf("Socket created successfully.\n");

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

    printf("Bind successful.\n");

    if (listen(server_fd, 5) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

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

        // Request username from client
        send(client->socket, "Enter your username: ", 22, 0);

        // Receive username
        int username_bytes = recv(client->socket, client->username, USERNAME_SIZE - 1, 0);
        if (username_bytes <= 0)
        {
            printf("Failed to receive username.\n");
            free(client);
            continue;
        }
        client->username[username_bytes] = '\0';

        // Remove newline if present
        for (int i = 0; i < username_bytes; i++)
        {
            if (client->username[i] == '\n' || client->username[i] == '\r')
            {
                client->username[i] = '\0';
                break;
            }
        }

        printf("Client username: %s\n", client->username);

        add_client(client);

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