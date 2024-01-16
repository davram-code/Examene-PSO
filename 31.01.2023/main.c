#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "thread.h"

#define NR_THREADS 2

// Input file with commands
int fd_in;
int pipe_fd[2];

/*
struct sigevent sev;
sev.sigev_notify = SIGEV_SIGNAL;     
sev.sigev_signo = SIGRTMIN;          
sev.sigev_value.sival_ptr = &timerid;
*/

void child_signal_handler(int signal, siginfo_t info, void *data) {
    kill(getppid(), SIGKILL);
    exit(-1);
}

void child_func() {
    timer_t timers[NR_THREADS];
    struct sigevent sevs[NR_THREADS];
    struct itimerspec tspec[NR_THREADS];

    for (int i = 0; i < NR_THREADS; i++){
        sevs[i].sigev_notify = SIGEV_SIGNAL;
        sevs[i].sigev_signo  = SIGRTMIN;
        sevs[i].sigev_value.sival_ptr = &timers[i];

        timer_create(CLOCK_REALTIME,
                            &sevs[i],&timers[i]);

        
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = child_signal_handler;

    sigaction(SIGRTMIN, &sa, NULL);

    char buffer[1024];
    
    while (read(pipe_fd[0], buffer, 7)){
        if (memcmp(buffer+3, "STAR", 4) == 0)
        {   
            buffer[2] = NULL;
            int id = atoi(buffer);
            tspec[id].it_value.tv_sec = 5;
            tspec[id].it_value.tv_nsec = 0;
            tspec[id].it_interval.tv_sec = 0;
            tspec[id].it_interval.tv_nsec = 0;

            timer_settime(&timers[id], 0, &tspec[id], NULL);
        }

        if (memcmp(buffer+3, "STOP", 4) == 0)
        {
            buffer[2] = NULL;
            int id = atoi(buffer);
            tspec[id].it_value.tv_sec = 0;
            tspec[id].it_value.tv_nsec = 0;

            timer_settime(&timers[id], 0, &tspec[id], NULL);
        }
    }
}

void blocare_semnale() {
    sigset_t signal_set;
    sigfillset(&signal_set);
    sigdelset(&signal_set, SIGINT);
    sigdelset(&signal_set, SIGUSR1);

    sigprocmask(SIG_BLOCK, &signal_set, NULL);
}

int main(int argc, char** argv) {

    pthread_t tids[NR_THREADS];
    blocare_semnale();    
 
    if (argc != 2) {
        fprintf(stderr, "Number of params incorrect!\n");
        return -1;
    }

    fd_in = open(argv[1], O_RDONLY);
    if (-1 == fd_in) {
        perror("Error on opening params file!\n");
    }

    pipe(pipe_fd);

    pid_t child = fork();
    if (child == 0) {
        close(pipe_fd[1]);
        child_func();
        exit(0);
    } else if (child < 0) {
        perror("Child could not be created!");
        return -1;
    }
    close(pipe_fd[0]);

    for (int i = 0; i < NR_THREADS; i++){
        pthread_create(&tids[i], NULL, 
                        thread_func, NULL);
    }

    
    for(int i = 0; i < NR_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }


    return 0;
}