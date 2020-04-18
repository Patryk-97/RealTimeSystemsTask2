#include "message.h"

struct message_t* messageConstructor(int size, enum direction dir)
{
    // locals
    struct message_t* message = NULL;
    
    message = (struct message_t*)malloc(sizeof(struct message_t));
    
    if(message != NULL)
    {
        message->size = size;
        message->dir = dir;
        message->valid = 1;
    }
    
    return message;
}

void messageDestructor(struct message_t** message)
{
    if(message != NULL && (*message) != NULL)
    {
        free(*message);
        *message = NULL;
    }
}