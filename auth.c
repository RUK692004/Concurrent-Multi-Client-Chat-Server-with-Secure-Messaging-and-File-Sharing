#include <stdio.h>
#include <string.h>

#include "auth.h"

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