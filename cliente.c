#include "rdt3.0.h"

int main(int argc, char **argv)
{
    // Verifica se os argumentos corretos foram passados (IP e porta)
    if (argc != 3)
    {
        printf("%s <IP> <porta>\n", argv[0]);
        return 0;
    }

    // Criação do socket UDP
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (s < 0)
    {
        perror("socket()");
        return -1;
    }

    struct sockaddr_in saddr;

    // Configuração da estrutura de endereço
    saddr.sin_port = htons(atoi(argv[2])); // Converte a porta para o formato de rede
    saddr.sin_family = AF_INET;

    // Converte o endereço IP de string para formato binário
    if (inet_aton(argv[1], &saddr.sin_addr) < 0)
    {
        perror("inet_pton()");
        return -1;
    }

    int dynamic_window, dynamic_timer;
    float msec;

    // Pergunta ao usuário parametros da transmissao
    printf("Janela Dinamica?(1:0)\n");
    scanf("%d", &dynamic_window);

    printf("Timer Dinamico?(1:0)\n");
    scanf("%d", &dynamic_timer);

    printf("Tempo limite rtt(msec):\n");
    scanf("%f", &msec);

    // Abre o arquivo a ser enviado
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

    // Cria o arquivo de log para registrar a transmissão
    char filename[50];
    sprintf(filename, "log_%d_%d_%.0f.csv", dynamic_window, dynamic_timer, msec);
    FILE *csv_file = fopen(filename, "w");
    if (!csv_file)
    {
        perror("Erro ao abrir o arquivo");
        return -1;
    }

    // Envia o tamanho do arquivo para o servidor
    rdt_send(s, &file_size, sizeof(file_size), &saddr, dynamic_window, dynamic_timer, msec, csv_file);

    // aloca o buffer
    char buffer[BUFFER_SIZE];
    long total_sent = 0;

    struct timeval inicio, fim;
    gettimeofday(&inicio, NULL); // Marca o tempo de início da transmissão
    int sends = 0;

    // Loop para enviar o arquivo em partes (tamanho máximo definido pelo buffer)
    while (total_sent < file_size)
    {
        int bytes_to_send = (file_size - total_sent > BUFFER_SIZE) ? BUFFER_SIZE : file_size - total_sent;

        // Lê uma parte do arquivo para o buffer
        fread(buffer, 1, bytes_to_send, file);

        // Registra no log
        fprintf(csv_file, "\nEnvio: %d\n", sends);
        fflush(csv_file);

        // Envia a parte do arquivo para o servidor
        rdt_send(s, buffer, bytes_to_send, &saddr, dynamic_window, dynamic_timer, msec, csv_file);

        total_sent += bytes_to_send;
        sends++;
    }

    gettimeofday(&fim, NULL); // Marca o tempo de fim da transmissão

    // Calcula o tempo total de transmissão
    float exec_time = time_diff(&inicio, &fim);

    printf("Transmissão concluída em %.3f msec\n", exec_time);

    // Registra os detalhes da transmissão no log
    fprintf(csv_file, "\nJanela Dinamica: %d, Timer Dinamico: %d, msec inicial: %.3f, Tamanho max Janela: %d, Tempo total de transmissão: %.3f\n", dynamic_window, dynamic_timer, msec, MAX_WINDOW_SIZE, exec_time);
    fprintf(csv_file, "----------------------------");

    // Fecha os arquivos
    fclose(csv_file);
    fclose(file);

    return 0;
}
