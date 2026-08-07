#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>

#include "rooms.h"

void init_rooms()
{
    strcpy(rooms[0].name, "Lobby");
    room_count = 1;
}

int room_exists(char *name)
{
    for (int i = 0; i < room_count; i++)
    {
        if (strcmp(rooms[i].name, name) == 0)
        {
            return 1;
        }
    }
    return 0;
}

int create_room(char *name)
{
    if (room_exists(name))
    {
        return 0;
    }

    if (room_count < MAX_ROOMS)
    {
        strcpy(rooms[room_count].name, name);
        room_count++;
        return 1;
    }

    return -1;
}

void list_rooms(int socket)
{
    char response[BUFFER_SIZE];
    sprintf(response, "SYS|Available Rooms:\n");
    send(socket, response, strlen(response), 0);

    for (int i = 0; i < room_count; i++)
    {
        char room_line[BUFFER_SIZE];
        sprintf(room_line, "SYS|  %s\n", rooms[i].name);
        send(socket, room_line, strlen(room_line), 0);
    }
}