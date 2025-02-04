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

    FILE *file = fopen("imagem_recebida.png", "wb");
    if (!file)
    {
        perror("Erro ao criar o arquivo");
        return;
    }
    char buffer[MSG_LEN];

    rdt_recv(s, &buffer, sizeof(buffer), &caddr);

    fwrite(buffer, 1, MSG_LEN, file);

    unsigned char md5_result[MD5_DIGEST_LENGTH];
    compute_md5((unsigned char *)buffer, strlen(buffer), md5_result);
    print_md5(md5_result);

    return 0;
}