#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <semaphore.h>
#include "message.h"

#define SEMAPHORE1_NAME "/semaphore1"
#define SEMAPHORE2_NAME "/semaphore2"

#define SEMAPHORE_DESTROY(sem, semName) \
    do \
    { \
        sem_close(sem); \
        sem_unlink(semName); \
    } \
    while(0) 

#define PIPE_READ 0
#define PIPE_WRITE 1

sem_t* semafor1;
sem_t* semafor2;

int clientService(int* pipefd);
int serverService(int* pipefd);

int greaterComparator(int a, int b)
{
    return a > b;
}

int lessComparator(int a, int b)
{
    return a < b;
}

int main(int argc, char *argv[])
{
    semafor1 = sem_open(SEMAPHORE1_NAME, O_CREAT, 0644, 1);
    semafor2 = sem_open(SEMAPHORE2_NAME, O_CREAT, 0644, 0);
    int pipefd[2];
    pid_t pid;
    if(semafor1 == SEM_FAILED)
    {
        printf("Semafor1 creation failure\n");
        return EXIT_FAILURE;
    }
    if(semafor2 == SEM_FAILED)
    {
        SEMAPHORE_DESTROY(semafor1, SEMAPHORE1_NAME);
        printf("Semafor2 creation failure\n");
        return EXIT_FAILURE;
    }
    srand(time(NULL));

    if (pipe(pipefd) == -1)
    {
        SEMAPHORE_DESTROY(semafor1, SEMAPHORE1_NAME);
        SEMAPHORE_DESTROY(semafor2, SEMAPHORE2_NAME);
        printf("Pipe init failure\n");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid == -1)
    {
        SEMAPHORE_DESTROY(semafor1, SEMAPHORE1_NAME);
        SEMAPHORE_DESTROY(semafor2, SEMAPHORE2_NAME);
        printf("Fork failure\n");
        return EXIT_FAILURE;
    }

    if (pid != 0)
    {
        // Server
        int serverExitFlag = serverService(pipefd);
        wait(NULL);                /* Wait for child */
        SEMAPHORE_DESTROY(semafor1, SEMAPHORE1_NAME);
        SEMAPHORE_DESTROY(semafor2, SEMAPHORE2_NAME);
        return serverExitFlag;
    }
    else
    {
        // Client
        int clientExitFlag = clientService(pipefd);
        _exit(clientExitFlag);
    }
    
    return EXIT_SUCCESS;
}

int clientService(int* pipefd)
{
    // Client
    const char* sharedMemoryObjectName = "/shared_memory_object";
    struct message_t* message = messageConstructor(10, ASCENDING);
    struct message_t *messageReply = messageConstructor(0, NOT_DEFINED);
    strcpy(message->messageBuff, sharedMemoryObjectName);

    sem_wait(semafor1);
    
    //file descriptor
    int fd = shm_open( sharedMemoryObjectName, O_RDWR | O_CREAT, 0600 );
    
    if(fd == -1)
    {
        printf("[Client] Cannot create shared memory object\n");
        message->valid = 0;
        close(pipefd[PIPE_READ]);
        write(pipefd[PIPE_WRITE], message, sizeof(struct message_t));
        close(pipefd[PIPE_WRITE]);
        sem_post(semafor2);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    if(ftruncate(fd, message->size * sizeof(int)) == -1)
    {
        printf("[Client] Cannot truncate file descriptor of shared memory object\n");
        message->valid = 0;
        close(pipefd[PIPE_READ]);
        write(pipefd[PIPE_WRITE], message, sizeof(struct message_t));
        close(pipefd[PIPE_WRITE]);
        shm_unlink(sharedMemoryObjectName);
        sem_post(semafor2);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    int* addr = (int*) mmap(NULL, message->size * sizeof(int), 
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        
    if(addr == MAP_FAILED)
    {
        printf("[Client] Map failed\n");
        message->valid = 0;
        close(pipefd[PIPE_READ]);
        write(pipefd[PIPE_WRITE], message, sizeof(struct message_t));
        close(pipefd[PIPE_WRITE]);
        shm_unlink(sharedMemoryObjectName);
        close(fd);
        sem_post(semafor2);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    for(int i = 0; i < message->size; i++)
    {
        addr[i] = rand()%100;
    }
    
    for(int i = 0; i < message->size; i++)
    {
        printf("%d ", addr[i]);
    }
    putchar('\n');
    
    write(pipefd[PIPE_WRITE], message, sizeof(struct message_t));
    close(pipefd[PIPE_WRITE]);
    sem_post(semafor2);
    
    sem_wait(semafor1);

    read(pipefd[PIPE_READ], messageReply, sizeof(struct message_t));
    close(pipefd[PIPE_READ]);
    
    if(messageReply->valid == 0)
    {
        munmap(addr, message->size * sizeof(int));
        close(fd);
        shm_unlink(sharedMemoryObjectName);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    printf("Reply msg from server: %s\n", messageReply->messageBuff);
    
    for(int i = 0; i < message->size; i++)
    {
        printf("%d ", addr[i]);
    }
    putchar('\n');
    
    if(munmap(addr, message->size * sizeof(int)) == -1)
    {
        printf("[Client] Unmap failed\n");
        close(fd);
        shm_unlink(sharedMemoryObjectName);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    close(fd);

    shm_unlink(sharedMemoryObjectName);
    messageDestructor(&message); messageDestructor(&messageReply);
    
    return EXIT_SUCCESS;
}

int serverService(int* pipefd)
{
    // Server
    struct message_t* message = messageConstructor(0, NOT_DEFINED);
    struct message_t *messageReply = messageConstructor(0, NOT_DEFINED);
    sem_wait(semafor2);
    read(pipefd[PIPE_READ], message, sizeof(struct message_t));
    close(pipefd[PIPE_READ]);
    if(message->valid == 0)
    {
        close(pipefd[PIPE_WRITE]);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    printf("Msg from client: %s\n", message->messageBuff);

    int fd = shm_open( message->messageBuff, O_RDWR, 0600 );
    
    if(fd == -1)
    {
        printf("[Server] Cannot open shared memory object\n");
        messageReply->valid = 0;
        write(pipefd[PIPE_WRITE], messageReply, sizeof(struct message_t));
        close(pipefd[PIPE_WRITE]);
        sem_post(semafor1);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    int* addr = (int*) mmap(0, message->size * sizeof(int),
        PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        
    if(addr == MAP_FAILED)
    {
        printf("[Server] Map failed\n");
        messageReply->valid = 0;
        write(pipefd[PIPE_WRITE], messageReply, sizeof(struct message_t));
        close(pipefd[PIPE_WRITE]);
        close(fd);
        sem_post(semafor1);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    int (*comparator)(int, int) = 
        (message->dir == ASCENDING ? greaterComparator : lessComparator);
    for(int i = message->size; i > 0; --i)
    {
        for(int j = 0; j < message->size - 1; ++j)
        {
            if(comparator(addr[j], addr[j + 1]))
            {
                int temp = addr[j];
                addr[j] = addr[j + 1];
                addr[j + 1] = temp;
            }
        }
    }
    
    if(munmap(addr, message->size * sizeof(int)) == -1)
    {
        printf("[Server] unmap failed\n");
        messageReply->valid = 0;
        write(pipefd[PIPE_WRITE], messageReply, sizeof(struct message_t));
        close(pipefd[PIPE_WRITE]);
        close(fd);
        sem_post(semafor1);
        messageDestructor(&message); messageDestructor(&messageReply);
        return EXIT_FAILURE;
    }
    
    close(fd);
    
    strcpy(messageReply->messageBuff, "Done");
    write(pipefd[PIPE_WRITE], messageReply, sizeof(struct message_t));
    close(pipefd[PIPE_WRITE]);
    sem_post(semafor1);
    
    messageDestructor(&message); messageDestructor(&messageReply);
    return EXIT_SUCCESS;
}