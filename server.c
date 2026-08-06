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

// Authenticate user
int authenticate(char *username, char *password)
{
    FILE *fp = fopen("users.txt", "r");

    if (fp == NULL)
    {
        perror("users.txt");
        return 0;
    }

    char line[100];

    while (fgets(line, sizeof(line), fp))
    {
        char *u = strtok(line, ",");
        char *p = strtok(NULL, ",");

        if (u == NULL || p == NULL)
            continue;

        p[strcspn(p, "\n")] = '\0';

        if (strcmp(username, u) == 0 &&
            strcmp(password, p) == 0)
        {
            fclose(fp);
            return 1;
        }
    }

    fclose(fp);

    return 0;
}

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
            printf(YELLOW "[INFO] %s requested disconnect.\n" RESET, client->username);

            char msg[100];
            sprintf(msg, "%s left the chat.", client->username);
            broadcast(msg, client_socket);

            remove_client(client_socket);
            close(client_socket);
            pthread_exit(NULL);
        }

        broadcast(buffer, client_socket);
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