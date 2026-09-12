#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

#include "common.h"

void *shared_mem = NULL;
cam_entry *cam_entries = NULL;
sem_t *cam_sem = NULL;

datagram dg_buffer[BUFFER_SIZE];
int dg_buffer_read_end = 0;
int dg_buffer_write_end = 0;
sem_t sem_dg_buffer_full;
sem_t sem_dg_buffer_empty;
pthread_mutex_t mutex_dg_buffer;

int port_sockets[NR_PORTS];

void init()
{
    sem_init(&sem_dg_buffer_full, 0, 0);
    sem_init(&sem_dg_buffer_empty, 0, BUFFER_SIZE);

    pthread_mutex_init(&mutex_dg_buffer, NULL);

    shared_mem = mmap(NULL, 10 * getpagesize(), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

    cam_sem = shared_mem;
    sem_init(cam_sem, 1, 1);

    cam_entries = shared_mem + sizeof(cam_sem);

    for (int i = 0; i < CAM_SIZE; i++)
    {
        memset(&cam_entries[i].client_sock_addr, 0, sizeof(cam_entries[i].client_sock_addr));
        cam_entries[i].port = -1;
        cam_entries[i].time = -1;
    }
}

void *switching_thread(void *param)
{
    UNUSED_PARAM(param);

    datagram send_buffer;
    while (1)
    {
        sem_wait(&sem_dg_buffer_full);
        pthread_mutex_lock(&mutex_dg_buffer);
        memcpy(&send_buffer, &dg_buffer[dg_buffer_read_end], sizeof(datagram));
        dg_buffer_read_end++;
        dg_buffer_read_end %= CAM_SIZE;
        sem_post(&sem_dg_buffer_empty);
        pthread_mutex_unlock(&mutex_dg_buffer);

        char sock_name[128];
        struct sockaddr_un addr;

        sem_wait(cam_sem);
        sprintf(sock_name, "/tmp/port_%d", cam_entries[send_buffer.dst].port);
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_name, sizeof(addr.sun_path) - 1);

        int port_to_send = cam_entries[send_buffer.dst].port;
        if (port_to_send != -1)
        {
            sendto(port_sockets[cam_entries[send_buffer.dst].port], &send_buffer, sizeof(datagram), 0, ((const struct sockaddr *)&cam_entries[send_buffer.dst].client_sock_addr), sizeof(addr));
        }
        else
        {
            for (int i = 0; i < NR_PORTS; i++)
            {
                if (i == send_buffer.src)
                {
                    continue;
                }

                // This will fail most of the time because if there are will be no clients registered for some ports.
                sendto(port_sockets[i], &send_buffer, sizeof(datagram), 0, ((const struct sockaddr *)&cam_entries[i].client_sock_addr), sizeof(addr));
            }
        }
        sem_post(cam_sem);
        pthread_mutex_unlock(&mutex_dg_buffer);
    }

    return NULL;
}

void *port_read_thread(void *param)
{
    int port_nr = *((int *)param);
    free(param);
    param = NULL;

    datagram read_buffer;
    struct sockaddr_un addr;
    int size;
    socklen_t addr_len;

    while (1)
    {
        size = sizeof(read_buffer);
        addr_len = sizeof(struct sockaddr_un);

        recvfrom(port_sockets[port_nr], &read_buffer, size, 0, ((struct sockaddr *)&addr), &addr_len);

        sem_wait(&sem_dg_buffer_empty);
        pthread_mutex_lock(&mutex_dg_buffer);
        memcpy(&dg_buffer[dg_buffer_write_end], &read_buffer, sizeof(datagram));
        dg_buffer_write_end++;
        dg_buffer_write_end %= CAM_SIZE;

        sem_wait(cam_sem);
        cam_entries[read_buffer.src].client_sock_addr = addr;
        cam_entries[read_buffer.src].port = port_nr;
        cam_entries[read_buffer.src].time = 10;
        sem_post(cam_sem);

        sem_post(&sem_dg_buffer_full);
        pthread_mutex_unlock(&mutex_dg_buffer);
    }

    return NULL;
}

void create_sockets()
{
    struct sockaddr_un addr;

    char sock_name[128];
    for (int i = 0; i < NR_PORTS; i++)
    {
        sprintf(sock_name, "%s%d", SOCK_NAME_PREFIX, i);
        port_sockets[i] = socket(AF_UNIX, SOCK_DGRAM, 0);

        memset(&addr, 0, sizeof(struct sockaddr_un));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, sock_name, sizeof(addr.sun_path) - 1);

        if (access(sock_name, F_OK) == 0)
        {
            if (unlink(sock_name) == -1)
            {
                perror("unlink");
                exit(EXIT_FAILURE);
            }
        }

        if (bind(port_sockets[i], (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1)
        {
            printf("unix sock failed for %s\n", sock_name);
            exit(-1);
        }
    }
}

#ifdef __SIGACTION__
void sig_handler_child(int sig, siginfo_t *siginfo, void *data)
{
    UNUSED_PARAM(sig);
    UNUSED_PARAM(siginfo);
    UNUSED_PARAM(data);
#else
void sig_handler_child(int signum)
{
    UNUSED_PARAM(signum);
#endif
    printf("Received sigterm\n");
    exit(0);
}

int main()
{

    init();
    pid_t aging_proc_pid;

    int pipefd[2];
    pipe(pipefd);

    aging_proc_pid = fork();
    if (aging_proc_pid == 0)
    {
        close(pipefd[0]);

#ifdef __SIGACTION__
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));

        sa.sa_handler = sig_handler_child;
        sigaction(SIGTERM, &sa, NULL);
#else
        signal(SIGTERM, sig_handler_child);
#endif
        while (1)
        {
            sem_wait(cam_sem);
            for (int i = 0; i < CAM_SIZE; i++)
            {
                if (cam_entries[i].port == -1)
                    continue;
                else
                {
                    cam_entries[i].time--;
                    if (cam_entries[i].time == 0)
                    {
                        char buffer[16];
                        sprintf(buffer, "PORT %d SCOS\n", cam_entries[i].port);
                        cam_entries[i].port = -1;
                        write(pipefd[1], buffer, strlen(buffer) + 1);
                    }
                }
            }

            sem_post(cam_sem);
            sleep(1);
        }

        exit(0);
    }
    else if (aging_proc_pid < 0)
    {
        printf("can't create child");
        exit(-1);
    }
    close(pipefd[1]);

    create_sockets();

    pthread_t port_threads[NR_PORTS];
    pthread_t switching_thread_id;
    int *param[NR_PORTS];
    for (int i = 0; i < NR_PORTS; i++)
    {
        param[i] = malloc(sizeof(int));
        *param[i] = i;
        pthread_create(&port_threads[i], NULL, port_read_thread, param[i]);
    }

    pthread_create(&switching_thread_id, NULL, switching_thread, NULL);

    int epfd = epoll_create(1);
    struct epoll_event ev;

    ev.data.fd = STDIN_FILENO;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    ev.data.fd = pipefd[0];
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, pipefd[0], &ev);

    while (1)
    {
        struct epoll_event ret_ev;
        epoll_wait(epfd, &ret_ev, 1, -1);
        char buffer[16];

        if ((ret_ev.data.fd == pipefd[0]) && ((ret_ev.events & EPOLLIN) != 0))
        {
            read(pipefd[0], buffer, 16);
            printf(buffer);
        }
        else if ((ret_ev.data.fd == STDIN_FILENO) &&
                 ((ret_ev.events & EPOLLIN) != 0))
        {
            read(STDERR_FILENO, buffer, 16);
            if (strncmp(buffer, "exit", 4) == 0)
            {
                kill(aging_proc_pid, SIGTERM);
                wait(NULL);

                munmap(shared_mem, 10 * getpagesize());

                pthread_cancel(switching_thread_id);
                for (int i = 0; i < NR_PORTS; i++)
                {
                    pthread_cancel(port_threads[i]);
                    shutdown(port_sockets[i], SHUT_RDWR);
                    close(port_sockets[i]);
                    close(pipefd[0]);
                    char sock_name[16];
                    sprintf(sock_name, "/tmp/port_%d", i);
                    unlink(sock_name);
                }

                break;
            }
        }
    }
    printf("Good Bye!\n");
    return 0;
}