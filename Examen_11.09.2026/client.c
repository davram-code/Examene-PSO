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
#include <errno.h>

#include "common.h"

int main(int argc, char **argv)
{

    if (argc < 3)
    {
        printf("Usage: %s [client address] [port to connect]", argv[0]);
        exit(-1);
    }

    int sfd;
    int epfd;
    int my_address = atoi(argv[1]);
    int port = atoi(argv[2]);
    struct sockaddr_un srvaddr;
    struct epoll_event ev;
    struct sockaddr_un claddr;

    char sock_name[64];
    sprintf(sock_name, "%s%d", SOCK_NAME_PREFIX, port);

    sfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    memset(&claddr, 0, sizeof(struct sockaddr_un));
    claddr.sun_family = AF_UNIX;
    snprintf(claddr.sun_path, sizeof(claddr.sun_path),
             "%s.%ld", sock_name, (long)getpid());

    memset(&srvaddr, 0, sizeof(struct sockaddr_un));
    srvaddr.sun_family = AF_UNIX;
    strncpy(srvaddr.sun_path, sock_name, sizeof(srvaddr.sun_path) - 1);

    if (bind(sfd, (struct sockaddr *)&claddr, sizeof(struct sockaddr_un)) == -1)
    {
        printf("unix sock failed for %s\n", sock_name);
        exit(-1);
    }

    epfd = epoll_create(1);
    ev.data.fd = STDIN_FILENO;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);

    ev.data.fd = sfd;
    ev.events = EPOLLIN;
    epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);

    while (1)
    {
        struct epoll_event ret_ev;
        unsigned int addrlen;
        addrlen = sizeof(struct sockaddr_un);

        int dst_addr;
        char msg[256];
        datagram recv_datagram;
        datagram send_datagram = {
            .src = my_address};

        epoll_wait(epfd, &ret_ev, 1, -1);
        if (ret_ev.data.fd == STDIN_FILENO)
        {
            scanf("%d %s", &dst_addr, msg);
            printf("Sending to address %d message: %s\n", dst_addr, msg);
            send_datagram.dst = dst_addr;
            strcpy(send_datagram.data, msg);
            sendto(sfd, &send_datagram, sizeof(datagram), 0, (struct sockaddr *)&srvaddr, addrlen);
        }
        else
        {
            recvfrom(ret_ev.data.fd, &recv_datagram, 264, 0, NULL, NULL);
            printf("Received message from address %d: %s\n", recv_datagram.src, recv_datagram.data);
        }
    }

    return 0;
}