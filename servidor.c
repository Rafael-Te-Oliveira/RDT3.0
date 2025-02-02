#include "rdt3.0.h"

int main(int argc, char **argv)
{
    if (argc != 2)
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

    struct sockaddr_in saddr, caddr;

    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(atoi(argv[1]));
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind()");
        return -1;
    };

    int msg;
    int pck_recv = 0;
    while (1)
    {
        if ((rdt_recv(s, &msg, sizeof(msg), &caddr)))
        {
            pck_recv++;
        }
    }

    return 0;
}