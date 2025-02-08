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
    }

    FILE *file = fopen("imagem_recebida.png", "wb");
    if (!file)
    {
        perror("Erro ao criar o arquivo");
        return -1;
    }

    long file_size;

    // Recebe o tamanho do arquivo
    rdt_recv(s, &file_size, sizeof(file_size), &caddr);

    char buffer[BUFFER_SIZE];
    long total_received = 0;

    // Recebe o arquivo em partes
    while (total_received < file_size)
    {
        int bytes_to_receive = (file_size - total_received > BUFFER_SIZE) ? BUFFER_SIZE : file_size - total_received;

        rdt_recv(s, buffer, bytes_to_receive, &caddr);

        // Escreve no arquivo
        fwrite(buffer, 1, bytes_to_receive, file);

        total_received += bytes_to_receive;
    }

    fclose(file);
    return 0;
}
