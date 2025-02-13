#include "rdt3.0.h"

int main(int argc, char **argv)
{
    // Verifica se o argumento correto foi passado (porta)
    if (argc != 2)
    {
        printf("%s <porta>\n", argv[0]);
        return 0;
    }

    // seeder para randomizar corrupçao
    srand(time(NULL));

    // Criação do socket UDP
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

    // associa o socket ao endereço e porta definidos
    if (bind(s, (struct sockaddr *)&saddr, sizeof(saddr)) < 0)
    {
        perror("bind()");
        return -1;
    }

    // Cria o arquivo a ser recebido
    FILE *file = fopen("imagem_recebida.png", "wb");
    if (!file)
    {
        perror("Erro ao criar o arquivo");
        return -1;
    }

    long file_size;

    // Recebe o tamanho do arquivo
    rdt_recv(s, &file_size, sizeof(file_size), &caddr);

    // aloca o buffer
    char buffer[BUFFER_SIZE];
    long total_received = 0;

    // Loop para receber o arquivo em partes (tamanho máximo definido pelo buffer)

    while (total_received < file_size)
    {
        int bytes_to_receive = (file_size - total_received > BUFFER_SIZE) ? BUFFER_SIZE : file_size - total_received;

        rdt_recv(s, buffer, bytes_to_receive, &caddr);

        // Escreve no arquivo
        fwrite(buffer, 1, bytes_to_receive, file);

        total_received += bytes_to_receive;
    }
    // Fecha o arquivo
    fclose(file);
    return 0;
}
