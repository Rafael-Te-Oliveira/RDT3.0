#include "rdt3.0.h"
#include <openssl/md5.h>

void compute_md5(const unsigned char *data, size_t length, unsigned char *md5_result)
{
    MD5(data, length, md5_result); // Calcula o hash MD5
}

void print_md5(unsigned char *md5_result)
{
    printf("MD5: ");
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
    {
        printf("%02x", md5_result[i]); // Converte para hexadecimal
    }
    printf("\n");
}

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

    char msg[MSG_LEN] = "Teste de mensagem que sera fragmentada";

    rdt_send(s, &msg, sizeof(msg), &saddr);

    unsigned char md5_result[MD5_DIGEST_LENGTH];
    compute_md5((unsigned char *)msg, strlen(msg), md5_result);
    print_md5(md5_result);

    return 0;
}