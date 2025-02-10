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

    char filename[50];
    sprintf(filename, "log_%d_%d_%.3f.csv", dynamic_window, dynamic_timer, msec);

    FILE *csv_file = fopen(filename, "w");
    if (!csv_file)
    {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    // Envia o tamanho do arquivo para o servidor
    rdt_send(s, &file_size, sizeof(file_size), &saddr, dynamic_window, dynamic_timer, msec, csv_file);

    char buffer[BUFFER_SIZE];
    long total_sent = 0;

    struct timeval inicio, fim;
    gettimeofday(&inicio, NULL);
    int sends = 0;
    // Envia o arquivo em partes, com o tamanho máximo de buffer
    while (total_sent < file_size)
    {
        int bytes_to_send = (file_size - total_sent > BUFFER_SIZE) ? BUFFER_SIZE : file_size - total_sent;

        // Lê uma parte do arquivo para o buffer
        fread(buffer, 1, bytes_to_send, file);

        fprintf(csv_file, "\nEnvio: %d\n", sends);
        fflush(csv_file);

        // Envia a parte do arquivo
        rdt_send(s, buffer, bytes_to_send, &saddr, dynamic_window, dynamic_timer, msec, csv_file);

        total_sent += bytes_to_send;
        sends++;
    }

    gettimeofday(&fim, NULL);

    float exec_time = time_diff(&inicio, &fim);

    printf("Transmissão concluída em %.3f msec\n", exec_time);

    fprintf(csv_file, "\nJanela Dinamica: %d, Timer Dinamico: %d, msec inicial: %.3f, Tamanho max Janela: %d, Tempo total de transmissão: %.3f\n", dynamic_window, dynamic_timer, msec, MAX_WINDOW_SIZE, exec_time);
    fprintf(csv_file, "----------------------------");

    fclose(csv_file);

    fclose(file);
    return 0;
}
