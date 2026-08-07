#ifndef ROOMS_H
#define ROOMS_H

#include "utils.h"

void init_rooms();
int room_exists(char *name);
int create_room(char *name);
void list_rooms(int socket);

#endif