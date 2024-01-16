#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#define NR_PROC     3
#define NR_COMM     5
#define NR_THREADS  2
#define PIPE_READ   0
#define PIPE_WRITE  1

#define OP_MEMORY_STORE   0
#define OP_FILE_STORE     1
#define OP_SEND_SIGNAL    2

// pipe global
int pipe_fd[2];
int file_out;

char lista_comenzi[10][11];
char lista_mem[100][10];
int start, stop;

sem_t sem_full, sem_empty;

pthread_mutex_t mutex_comenzi = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_lista_mem = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_store_fisier = PTHREAD_MUTEX_INITIALIZER;


void *thread_func(void *params) {
    while(1) {
        char buffer[11];
        sem_wait(&sem_full);
        pthread_mutex_lock(&mutex_comenzi);
        memcpy(buffer, lista_comenzi[start], 11);
        start = (start + 1) % 10;
        pthread_mutex_unlock(&mutex_comenzi);
        sem_post(&sem_empty);

        char *saveptr;

        int pid = atoi(strtok_r(buffer, "_", &saveptr));
        int op = atoi(strtok_r(NULL, "_", &saveptr));
        char *str = strtok_r(NULL, "_", &saveptr);

        switch (op)
        {
        case OP_MEMORY_STORE:

            break;        
        case OP_FILE_STORE:
            break;
        case OP_SEND_SIGNAL:
            break;
        default:
            fprintf(stderr, "Bad Operation!\n");
            break;
        }

    }

    
}

char *rand_string(int size){
    char *str = malloc(size + 1);
    memset(str, '\0', size + 1);
    for (int i =0; i < size; i++) {
        str[i] = 'a' + rand() % 26;
    }
    return str;
}

void store(int op) {
    pid_t pid = getpid();
    char buffer[11];

    int bytes = 
        sprintf(buffer, "%d_%d_", pid, op);

    char *str = rand_string(10 - bytes);
    memcpy(buffer + bytes, str, 10 - bytes);
    
    write(pipe_fd[PIPE_WRITE], buffer, 10);
}

void send_signal() {
    pid_t pid = getpid();
    char buffer[11];
    
    int send_pid = 0;
    int bytes = 
        sprintf(buffer, "%d_%d_", pid, send_pid);

    for(int i = bytes; i<10;i++)
        buffer[i] = ' ';

    write(pipe_fd[PIPE_WRITE], buffer, 10);
}


void child_func(int out) {
    for (int i = 0; i < NR_COMM; i++) {
        int msec = rand() % 1000;
        
        struct timespec ts;
        ts.tv_nsec = msec * 1000000;
        ts.tv_sec = 0;
        nanosleep(&ts, NULL);

        int op = rand() % 3;

        switch (op)
        {
        case OP_MEMORY_STORE:
            store(op);
            break;        
        case OP_FILE_STORE:
            store(op);
            break;
        case OP_SEND_SIGNAL:
            send_signal();
            break;
        default:
            fprintf(stderr, "Bad Operation!\n");
            break;
        }
    } 



}
int main(int argc, char**argv) {
    pid_t pids[NR_PROC];
    sem_init(&sem_full, 1, 0);
    sem_init(&sem_empty, 1, 10);

    file_out = open(argv[1], O_WRONLY | O_APPEND | O_CREAT, 660);
    if (file_out == -1){
        perror("Failed opening file!");
        exit(-1);
    }

    srand(time(NULL));
    pipe(pipe_fd);


    for (int i = 0; i < NR_PROC; i++) {
        pids[i] = fork();

        switch(pids[i]) {
            case 0: 
                close(pipe_fd[PIPE_READ]);
                child_func(pipe_fd[PIPE_WRITE]);
                close(pipe_fd[PIPE_WRITE]); // optional
                exit(0);
            case -1:
                perror("Child could not be created!\n");
                exit(-1);
            default:
                ;
        }
    }
    
    close(pipe_fd[PIPE_WRITE]);


    pthread_t thread_id[NR_THREADS];
    for(int i = 0; i < NR_THREADS; i++) {
        pthread_create(&thread_id[i], NULL,
                            thread_func, NULL);
    }
    
    char buffer[11];
    buffer[10] = '\0';
    while (read(pipe_fd[PIPE_READ], buffer, 10)) {
        sem_wait(&sem_empty);
        pthread_mutex_lock(&mutex_comenzi);
        memcpy(lista_comenzi[stop], buffer, 11);
        stop = (stop + 1) % 10;
        pthread_mutex_unlock(&mutex_comenzi);
        sem_post(&sem_full);
    }

    // TODO: MAYBE ASTEPT THREADURILE


    for (int i = 0; i< NR_PROC; i++) {
        int status;
        waitpid(pids[i], &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Process %d exited normally!\n", pids[i]);
        }
    }

    return 0;
}