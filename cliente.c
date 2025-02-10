#include "rdt3.0.h"

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("%s <IP> <porta>\n", argv[0]);
        return 0;
    }

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

    int dynamic_window, dynamic_timer;
    float msec;

    printf("Janela Dinamica?(1:0)\n");
    scanf("%d", &dynamic_window);

    printf("Timer Dinamico?(1:0)\n");
    scanf("%d", &dynamic_timer);

    printf("Tempo limite rtt(msec):\n");
    scanf("%f", &msec);

    FILE *file = fopen("imagem_enviada.png", "rb");

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
    rdt_send(s, &file_size, sizeof(file_size), &saddr, dynamic_window, dynamic_timer, msec);

    char buffer[BUFFER_SIZE];
    long total_sent = 0;

    struct timeval inicio, fim;
    gettimeofday(&inicio, NULL);

    // Envia o arquivo em partes, com o tamanho máximo de buffer
    while (total_sent < file_size)
    {
        int bytes_to_send = (file_size - total_sent > BUFFER_SIZE) ? BUFFER_SIZE : file_size - total_sent;

        // Lê uma parte do arquivo para o buffer
        fread(buffer, 1, bytes_to_send, file);

        // Envia a parte do arquivo
        rdt_send(s, buffer, bytes_to_send, &saddr, dynamic_window, dynamic_timer, msec);

        total_sent += bytes_to_send;
    }

    gettimeofday(&fim, NULL);

    float exec_time = time_diff(&inicio, &fim);

    printf("Transmissão concluída em %.3f msec\n", exec_time);
    fclose(file);
    return 0;
}
