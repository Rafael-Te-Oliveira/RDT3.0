#include "rdt3.0.h"

float estimated_rtt = 100; // primeiro valor de estimated_rtt (arbitrário)
float dev_rtt = 0;         // primeiro valor de dev_rtt

// inicializacao das bases e do numero de sequencia
static hseq_t rcv_base = 0;
static hseq_t snd_base = 0;
static hseq_t next_seqnum = 0;

// retorna a diferença entre start e end
float time_diff(struct timeval *start, struct timeval *end)
{
    return ((end->tv_sec - start->tv_sec) * 1000) + (end->tv_usec - start->tv_usec) / 1000.0;
}

// atualiza msec de acordo com o rtt
float update_timeout(float sample_rtt)
{
    estimated_rtt = (1 - ALPHA) * estimated_rtt + ALPHA * sample_rtt;
    dev_rtt = (1 - BETA) * dev_rtt + BETA * fabs(sample_rtt - estimated_rtt);
    return estimated_rtt + 4 * dev_rtt;
}

// define o timer de recebimento
void settimer(int s, struct timeval timeout, float msec)
{
    timeout.tv_sec = (int)(msec / 1000);
    timeout.tv_usec = (int)((msec - (timeout.tv_sec * 1000)) * 1000);

    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt()");
    }
}

// calcula o checksum
unsigned short checksum(unsigned short *buf, int nbytes)
{
    register long sum = 0;
    while (nbytes > 1)
    {
        sum += *(buf++);
        nbytes -= 2;
    }
    if (nbytes == 1)
        sum += *(unsigned char *)buf;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (unsigned short)~sum;
}

// verifica corrupção
int iscorrupted(pkt *pr)
{
    pkt pl = *pr;
    pl.h.csum = 0;
    unsigned short csuml = checksum((void *)&pl, pl.h.pkt_size);
    return csuml != pr->h.csum;
}

// simula uma corrupcao aleatoria
void corrupt_packet(pkt *p)
{
    int corrupt = (rand() % 10);
    if (corrupt > 7)
        p->h.csum = 0;
}

// simula atraso de pacote
void delay_ack()
{
    int delay = (rand() % 10);
    usleep(delay * 10000);
}

// verifica se o seqnum recebido esta dentro da janela
int in_window(hseq_t seq, hseq_t base, int window_size)
{
    return (seq >= base && seq < base + window_size) ||
           (base + window_size > MAX_SEQ_NUM && (seq >= base || seq < (base + window_size) % MAX_SEQ_NUM));
}

// cria o pacote
int make_pkt(pkt *p, htype_t type, hseq_t seqnum, void *msg, int msg_len)
{

    if (msg_len > MMS)
    {
        return ERROR;
    }

    p->h.pkt_size = sizeof(hdr);
    p->h.csum = 0;
    p->h.pkt_type = type;
    p->h.pkt_seq = seqnum;
    if (msg_len > 0)
    {
        p->h.pkt_size += msg_len;
        memset(p->msg, 0, MMS);
        memcpy(p->msg, msg, msg_len);
    }
    p->h.csum = checksum((unsigned short *)p, p->h.pkt_size);
    return SUCCESS;
}

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst, int dynamic_window, int dynamic_timer, float msec, FILE *csv_file)
{
    fprintf(csv_file, "rtt;msec\n");
    fflush(csv_file);
    static snd_window send_window[MAX_WINDOW_SIZE]; // inicializa a janela com tamanho maximo da janela

    int window_size = (dynamic_window) ? 1 : MAX_WINDOW_SIZE; // verifica parametro passado de janela dinamica e define tamanho inicial da janela

    // inicializa variaveis de controle/log
    struct timeval timeout, start, end;
    int addrlen = sizeof(struct sockaddr_in);
    pkt ack;

    int total_packets = (buf_len + MMS - 1) / MMS;
    int sent_packets = 0;
    int confirmed_packets = 0;
    int envios_simultaneos = 0;
    int acked_count = 0;
    int retransmitted_packets = 0;

    float sample_rtt;

    // enquanto nao receber o ack de todos os pacotes do buffer
    while (confirmed_packets < total_packets)
    {
        // enquanto existir pacotes prontos para envio na janela
        while ((next_seqnum - snd_base + MAX_SEQ_NUM) % MAX_SEQ_NUM < window_size && sent_packets < total_packets)
        {
            // calcula offset e tamanho do bloco para fragmentacao da mensagem
            int offset = sent_packets * MMS;
            int chunk_size = (offset + MMS > buf_len) ? buf_len - offset : MMS;

            // cria o pacote com o bloco de mensagem e o vincula a janela
            make_pkt(&send_window[next_seqnum % MAX_WINDOW_SIZE].packet, PKT_DATA, next_seqnum, (char *)buf + offset, chunk_size);

            // pacote nao foi confirmado ainda
            send_window[next_seqnum % MAX_WINDOW_SIZE].acked = 0;

            gettimeofday(&send_window[next_seqnum % MAX_WINDOW_SIZE].send_time, NULL);

            // envia o pacote para o servidor
            sendto(sockfd, &send_window[next_seqnum % MAX_WINDOW_SIZE].packet,
                   send_window[next_seqnum % MAX_WINDOW_SIZE].packet.h.pkt_size,
                   MSG_CONFIRM, (struct sockaddr *)dst, addrlen);

            printf("Sent packet SeqNum: %d\n", next_seqnum);

            // incrementa variaveis de controle/log
            next_seqnum = (next_seqnum + 1) % MAX_SEQ_NUM;
            sent_packets++;
            envios_simultaneos++;
            printf("Simultaneous sending: %d\n", envios_simultaneos);
        }

        // define o timer para o recebimento do ack
        settimer(sockfd, timeout, msec);

        // espera o recebimento do ack
        int nr = recvfrom(sockfd, &ack, sizeof(pkt), MSG_WAITALL,
                          (struct sockaddr *)dst, (socklen_t *)&addrlen);

        // se o ack chegou a tempo e nao esta corrompido
        if (nr > 0 && !iscorrupted(&ack) && ack.h.pkt_type == PKT_ACK)
        {
            int ack_seq = ack.h.pkt_seq;

            // verifica se o ack pertence a janela e se nao é duplicado
            if (in_window(ack_seq, snd_base, window_size) && !send_window[ack_seq % MAX_WINDOW_SIZE].acked)
            {
                // atualiza as variaveis de controle e confirma o ack na janela
                send_window[ack_seq % MAX_WINDOW_SIZE].acked = 1;
                confirmed_packets++;
                acked_count++;
                envios_simultaneos--;

                printf("ACK: %d successfully received!\n", ack_seq);
                printf("Confirmed packets: %d\n", confirmed_packets);

                // pega o tempo de envio do pacote da janela e o tempo de recebimento
                start = send_window[ack_seq % MAX_WINDOW_SIZE].send_time;
                gettimeofday(&end, NULL);

                // calcula o rtt amostrado
                sample_rtt = time_diff(&start, &end);

                // se o timer for dinamico, atualiza o msec
                if (dynamic_timer)
                    msec = update_timeout(sample_rtt);

                printf("rtt: %.3f\n", sample_rtt);
                fprintf(csv_file, "%.3f;", sample_rtt);
                fprintf(csv_file, "%.3f\n", msec);
                fflush(csv_file);
                printf("New msec: %.3f\n", msec);

                // desloca a base da janela de acordo com os recebimentos de ack
                while (send_window[snd_base % MAX_WINDOW_SIZE].acked)
                {
                    send_window[snd_base % MAX_WINDOW_SIZE].acked = 0;
                    snd_base = (snd_base + 1) % MAX_SEQ_NUM;
                }

                // incrementa a janela se for dinamica e tiver recebido acks = window_size
                if (window_size < MAX_WINDOW_SIZE && acked_count >= window_size && dynamic_window)
                {
                    acked_count = 0;
                    window_size++;
                    printf("Window size: %d\n", window_size);
                }
            }
        }
        else if (nr < 0) // se o timer estourou
        {
            printf("Timer expired!\n");

            // se a janela é dinamica, divide ao meio
            if (window_size > 1 && dynamic_window)
            {
                acked_count = 0;
                window_size = window_size / 2;
                printf("Window divided by 2: %d\n", window_size);
            }
            gettimeofday(&end, NULL);

            // percorre a janela recuperando o tempo de envio dos pacotes
            for (int i = snd_base; i != next_seqnum; i = (i + 1) % MAX_SEQ_NUM)
            {
                int idx = i % MAX_WINDOW_SIZE;
                float elapsed_time = time_diff(&send_window[idx].send_time, &end);

                // se o tempo calculado for maior que msec, retransmite
                if (!send_window[idx].acked && elapsed_time > msec)
                {
                    gettimeofday(&send_window[idx].send_time, NULL);
                    sendto(sockfd, &send_window[idx].packet, send_window[idx].packet.h.pkt_size,
                           MSG_CONFIRM, (struct sockaddr *)dst, addrlen);
                    printf("Retransmitting packet SeqNum: %d\n", send_window[idx].packet.h.pkt_seq);
                    retransmitted_packets++;
                }
            }
        }
        fflush(stdout);
    }
    fprintf(csv_file, "Pacotes enviados:%d\n", sent_packets);
    fprintf(csv_file, "Pacotes retransmitidos:%d\n", retransmitted_packets);
    fprintf(csv_file, "Pacotes confirmados:%d\n", confirmed_packets);
    fflush(csv_file);
    return 0;
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src)
{
    // inicializa janela e demais variaveis de controle
    static rcv_window recv_window[MAX_WINDOW_SIZE];

    pkt p, ack;
    int nr, addrlen = sizeof(struct sockaddr_in);
    int received_count = 0;
    int expected_packets = (buf_len + MMS - 1) / MMS;
    int offset = 0;

    // enquanto nao receber todos os pacotes com sucesso
    while (received_count < expected_packets)
    {
        // espera o proximo pacote
        nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr *)src, (socklen_t *)&addrlen);

        // simula uma chance de corrupcao
        corrupt_packet(&p);

        // se pacote for recebido sem problemas
        if (nr > 0 && !iscorrupted(&p) && p.h.pkt_type == PKT_DATA)
        {
            int seqnum = p.h.pkt_seq;
            // verifica se pacote esta dentro da janela de recebimento
            if (in_window(seqnum, rcv_base, MAX_WINDOW_SIZE))
            {
                int index = seqnum % MAX_WINDOW_SIZE;
                if (!recv_window[index].received) // se nao for pacote duplicado
                {
                    recv_window[index].packet = p;
                    recv_window[index].received = 1;
                    received_count++;
                }
                while (recv_window[rcv_base % MAX_WINDOW_SIZE].received) // desliza a janela conforme recebe pacotes em ordem
                {
                    memcpy(buf + offset, recv_window[rcv_base % MAX_WINDOW_SIZE].packet.msg, recv_window[rcv_base % MAX_WINDOW_SIZE].packet.h.pkt_size - sizeof(hdr));
                    recv_window[rcv_base % MAX_WINDOW_SIZE].received = 0;
                    rcv_base = (rcv_base + 1) % MAX_SEQ_NUM;
                    offset += (p.h.pkt_size - sizeof(hdr)); // calcula offset para consumo do pacote
                }
            }
            // cria o pacote de ack
            make_pkt(&ack, PKT_ACK, seqnum, NULL, 0);

            printf("ACK: %d\n", seqnum);
            // devolve o ack, independente do pacote estar na janela
            sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, addrlen);
        }
        fflush(stdout);
    }
    return 0;
}
