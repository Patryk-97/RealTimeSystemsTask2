#ifndef __MESSAGE_H__
#define __MESSAGE_H__

#include <stdlib.h>

#define MESSAGE_BUFF_SIZE 30

enum direction
{
    NOT_DEFINED,
    ASCENDING,
    DESCENDING
};

struct message_t
{
    char messageBuff[MESSAGE_BUFF_SIZE];
    int size;
    enum direction dir;
    int valid;
};

struct message_t* messageConstructor(int size, enum direction dir);
void messageDestructor(struct message_t** message);

#endif // __MESSAGE_H__