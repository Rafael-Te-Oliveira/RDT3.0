#include "rdt3.0.h"

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("%s <IP> <porta>\n", argv[0]);
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
    }

    FILE *file = fopen("arquivo_enviado.txt", "rb");

    if (!file)
    {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    // Obtém o tamanho do arquivo
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // Envia o tamanho do arquivo para o servidor
    rdt_send(s, &file_size, sizeof(file_size), &saddr);

    char buffer[MAX_BUFFER_SIZE];
    long total_sent = 0;

    struct timeval inicio, fim;
    gettimeofday(&inicio, NULL);

    // Envia o arquivo em partes, com o tamanho máximo de buffer
    while (total_sent < file_size)
    {
        int bytes_to_send = (file_size - total_sent > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : file_size - total_sent;

        // Lê uma parte do arquivo para o buffer
        fread(buffer, 1, bytes_to_send, file);

        // Envia a parte do arquivo
        rdt_send(s, buffer, bytes_to_send, &saddr);

        total_sent += bytes_to_send;
    }

    gettimeofday(&fim, NULL);

    float exec_time = time_diff(&inicio, &fim);

    printf("Transmissão concluída em %.3f msec\n", exec_time);
    fclose(file);
    return 0;
}
