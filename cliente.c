#include "rdt3.0.h"

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("%s <porta>\n", argv[0]);
        return 0;
    }
    srand(time(NULL));
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s < 0)
    {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in saddr;

    saddr.sin_port = htons(atoi(argv[2]));
    saddr.sin_family = AF_INET;

    if (inet_aton(argv[1], &saddr.sin_addr) < 0)
    {
        perror("inet_pton()");
        return -1;
    };

    char msg[10000];

    // int msg = 1000;
    int msg_send = 0;

    printf("TAMANHO DA MSG: %zu", sizeof(msg));

    rdt_send(s, &msg, sizeof(msg), &saddr);

    // while (msg_send < MSG_NUM)
    // {
    //     rdt_send(s, &msg, sizeof(msg), &saddr);
    //     msg_send++;
    // }
    return 0;
}