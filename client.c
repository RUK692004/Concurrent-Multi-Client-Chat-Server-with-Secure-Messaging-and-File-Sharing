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

int sock;
char username[USERNAME_SIZE];

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

        printf("\r%s\n", buffer);
        printf("%s > ", username);
        fflush(stdout);
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

        printf("%s > ", username);
        fflush(stdout);

        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
        {
            break;
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

    printf("Connected to server.\n");

    // Receive username prompt
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    recv(sock, buffer, BUFFER_SIZE - 1, 0);

    // Get username from user
    printf("%s", buffer);
    fgets(username, USERNAME_SIZE, stdin);
    username[strcspn(username, "\n")] = '\0';
    send(sock, username, strlen(username), 0);

    pthread_t send_thread;
    pthread_t receive_thread;

    pthread_create(&receive_thread, NULL, receive_messages, NULL);
    pthread_create(&send_thread, NULL, send_messages, NULL);

    pthread_join(send_thread, NULL);
    pthread_join(receive_thread, NULL);

    close(sock);

    return 0;
}