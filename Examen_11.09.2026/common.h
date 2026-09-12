#include <sys/un.h>

#define NR_PORTS  4
#define BUFFER_SIZE  10
#define CAM_SIZE  256

#define SOCK_NAME_PREFIX "/tmp/port_"

#define UNUSED_PARAM(param) (void)(param)
typedef struct
{
    int port;
    int time;
    struct sockaddr_un client_sock_addr;
} cam_entry;

typedef struct
{
    int dst;
    int src;
    char data[256];
} datagram;