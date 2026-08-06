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

int sock;
char username[USERNAME_SIZE];
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Thread to receive messages */
void *receive_messages(void *arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);

        if (bytes <= 0)
        {
            printf("\nDisconnected from server.\n");
            close(sock);
            exit(0);
        }

        buffer[bytes] = '\0';

        pthread_mutex_lock(&print_mutex);

        // Check message type
        if (strncmp(buffer, "SYS|", 4) == 0)
        {
            // System message (join/leave) - remove trailing newline
            buffer[bytes - 1] = '\0'; // Remove \n
            printf("\r" YELLOW "*** %s ***" RESET "\n\n", buffer + 4);
        }
        else if (strncmp(buffer, "MSG|", 4) == 0)
        {
            // User message - parse username and message
            char *sender = buffer + 4;
            char *msg_content = strchr(sender, '|');
            if (msg_content != NULL)
            {
                *msg_content = '\0';
                msg_content++; // Skip past '|'
                // Remove trailing newline from message
                msg_content[strlen(msg_content) - 1] = '\0';
                printf("\r" GREEN "%s" RESET " > %s\n\n", sender, msg_content);
            }
        }
        
        printf(CYAN "%s" RESET " > ", username);
        fflush(stdout);

        pthread_mutex_unlock(&print_mutex);
    }

    return NULL;
}

/* Thread to send messages */
void *send_messages(void *arg)
{
    char buffer[BUFFER_SIZE];

    while (1)
    {
        memset(buffer, 0, BUFFER_SIZE);

        pthread_mutex_lock(&print_mutex);
        printf(CYAN "%s" RESET " > ", username);
        fflush(stdout);
        pthread_mutex_unlock(&print_mutex);

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
        {
            break;
        }

        // Remove trailing newline
        buffer[strcspn(buffer, "\n")] = '\0';

        // Check for exit command
        if (strcmp(buffer, "exit") == 0)
        {
            printf(RED "Disconnecting...\n" RESET);
            send(sock, buffer, strlen(buffer), 0);
            close(sock);
            exit(0);
        }

        send(sock, buffer, strlen(buffer), 0);
    }

    return NULL;
}

int main()
{
    struct sockaddr_in server_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf(GREEN "Connected to server.\n" RESET);

    char buffer[BUFFER_SIZE];

    // Get username from user
    printf("Username: ");
    fflush(stdout);
    fgets(username, USERNAME_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0';

    // Get password from user
    printf("Password: ");
    fflush(stdout);
    char password[PASSWORD_SIZE];
    fgets(password, PASSWORD_SIZE, stdin);
    password[strcspn(password, "\n")] = '\0';

    // Send combined login message
    char login_msg[BUFFER_SIZE];
    sprintf(login_msg, "LOGIN|%s|%s", username, password);
    send(sock, login_msg, strlen(login_msg), 0);

    // Receive authentication result
    memset(buffer, 0, BUFFER_SIZE);
    int bytes = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes <= 0)
    {
        printf(RED "Server disconnected.\n" RESET);
        close(sock);
        return 1;
    }
    buffer[bytes] = '\0';

    if (strcmp(buffer, "LOGIN_FAIL") == 0)
    {
        printf(RED "Authentication failed. Invalid username or password.\n" RESET);
        close(sock);
        return 1;
    }
    else if (strcmp(buffer, "LOGIN_OK") == 0)
    {
        printf(GREEN "Authentication successful!\n" RESET);
    }

    pthread_t send_thread;
    pthread_t receive_thread;

    pthread_create(&receive_thread, NULL, receive_messages, NULL);
    pthread_create(&send_thread, NULL, send_messages, NULL);

    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sock);

    return 0;
}