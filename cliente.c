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

    FILE *file = fopen("imagem_enviada.png", "rb");

    if (!file)
    {
        perror("Erro ao abrir o arquivo");
        return;
    }

    // Obtém o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Envia o tamanho do arquivo para o servidor
    rdt_send(s, &file_size, sizeof(file_size), &saddr);

    char buffer[file_size];

    fread(buffer, 1, file_size, file);

    struct timeval inicio, fim;
    gettimeofday(&inicio, NULL);

    rdt_send(s, &buffer, sizeof(buffer), &saddr);

    gettimeofday(&fim, NULL);

    float exec_time = time_diff(&inicio, &fim);

    printf("Transmissão concluída em %.3f msec\n", exec_time);

    unsigned char md5_result[MD5_DIGEST_LENGTH];
    compute_md5((unsigned char *)buffer, strlen(buffer), md5_result);
    print_md5(md5_result);

    return 0;
}