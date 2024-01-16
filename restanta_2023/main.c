#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define CHILD_TO_PARENT 0
#define PARENT_TO_CHILD 1

#define PIPE_READ 0
#define PIPE_WRITE 1


int NR_THREADS;
int*** pipes;
char *key;
size_t output_size;
char *mem_in, *mem_out;

char* generate_key() {
    char *set = "abcdef1234567890";
    srand(time(NULL));
    char *ret = malloc(33);

    for (int i=0;i<32;i++){
        ret[i] = set[rand() % 16];
    }    

    ret[32] = '\0';

    return ret;
}

void *thread_func(void *param) {
    int index = *(int*)param;

    // bpt = (nr_block + nr_thr - 1) / nr_thr;
    int start, size;
    // start = index * nr_block * 16
    // size (bpt * 16)

    write(pipes[index][PARENT_TO_CHILD][PIPE_WRITE], mem_in + start, size);
    read(pipes[index][CHILD_TO_PARENT][PIPE_READ], mem_out + start, size);    

}

int main(int argc, char **argv) {

    if (argc != 4) {
        fprintf(stderr, "Nr. of params incorrect\n");
        exit(-1);
    }

    key = generate_key();

    int fd_in, fd_out;
    NR_THREADS = atoi(argv[2]);
    fd_in = open(argv[1], O_RDONLY);
    fd_out = open(argv[3], O_CREAT | O_WRONLY | O_TRUNC, 0600);

    struct stat statbuf;
    memset(&statbuf, 0, sizeof(struct stat));
    fstat(fd_in, &statbuf);
    size_t file_size = statbuf.st_size;
    
    output_size = ((file_size + 15) / 16) * 16;
    ftruncate(fd_out, output_size);
    ftruncate(fd_in, output_size);

    mem_in = mmap(NULL, output_size, PROT_READ, 
            MAP_SHARED, fd_in, 0);
    mem_out = mmap(NULL, output_size, PROT_WRITE, 
            MAP_SHARED, fd_out, 0);

    pipes = malloc(NR_THREADS * sizeof(int**));
    pid_t *pids = malloc(NR_THREADS * sizeof(pids));


    for(int i=0;i<NR_THREADS;i++) {
        pipes[i] = malloc(2*sizeof(int*));
        for (int j =0; j<2; j++) {
            pipes[i][j] = malloc(2*sizeof(int));
            pipe2(pipes[i][j], O_CLOEXEC);
            printf("%d - %d\n", pipes[i][j][0], pipes[i][j][1]);
        }
    }

    
    for (int i =0; i < NR_THREADS;i++) {
        pids[i] = fork();

        if (pids[i] == 0) {
            fcntl(pipes[i][CHILD_TO_PARENT][PIPE_WRITE],
                        F_SETFD, 0);
            fcntl(pipes[i][PARENT_TO_CHILD][PIPE_READ],
                        F_SETFD, 0);
            
            dup2(pipes[i][CHILD_TO_PARENT][PIPE_WRITE], STDOUT_FILENO);
            dup2(pipes[i][PARENT_TO_CHILD][PIPE_READ], STDIN_FILENO);

            execl("/usr/bin/openssl", "openssl", "enc",
                            "-aes-128-ecb", "-e", "-K", key, "-nopad", NULL);

            exit(-1);
        }
    }

    for (int i =0; i<NR_THREADS; i++) {
        close(pipes[i][CHILD_TO_PARENT][PIPE_WRITE]);
        close(pipes[i][PARENT_TO_CHILD][PIPE_READ]);
    }

    pthread_t *tids = malloc(NR_THREADS * sizeof(pthread_t));
    int *index = malloc(NR_THREADS * sizeof(int));

    for (int i = 0; i < NR_THREADS; i++) {
        index[i] = i;
        pthread_create(&tids[i], NULL, thread_func, &index[i]);
    }

    for (int i = 0; i < NR_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }

    return 0;
}