#include "rdt3.0.h"

float estimated_rtt = 100; // Tempo inicial em ms
float dev_rtt = 0;
float msec = 100;

static hseq_t rcv_base = 0; // Base da janela do receptor
static hseq_t snd_base = 0; // Base da janela do emissor
static hseq_t next_seqnum = 0;

float time_diff(struct timeval *start, struct timeval *end)
{
    return ((end->tv_sec - start->tv_sec) * 1000) + (end->tv_usec - start->tv_usec) / 1000.0;
}

float update_timeout(float sample_rtt)
{
    estimated_rtt = (1 - ALPHA) * estimated_rtt + ALPHA * sample_rtt;
    dev_rtt = (1 - BETA) * dev_rtt + BETA * fabs(sample_rtt - estimated_rtt);
    return estimated_rtt + 4 * dev_rtt;
}

void settimer(int s, struct timeval timeout, float msec)
{
    timeout.tv_sec = 0;            // segundos
    timeout.tv_usec = msec * 1000; // microssegundos
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0)
    {
        perror("setsockopt()");
    }
}

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

int iscorrupted(pkt *pr)
{
    pkt pl = *pr;
    pl.h.csum = 0;
    unsigned short csuml = checksum((void *)&pl, pl.h.pkt_size);
    return csuml != pr->h.csum;
}

// void corrupt_packet(pkt *p)
// {
//     int corrupt = (rand() % 10);
//     if (corrupt > 5)
//         p->h.csum = 0;
// }

// void delay_ack()
// {
//     int delay = (rand() % 10);
//     usleep(delay * 10000);
// }

int in_window(hseq_t seq, hseq_t base)
{
    return (seq >= base && seq < base + MAX_WINDOW_SIZE) ||
           (base + MAX_WINDOW_SIZE > MAX_SEQ_NUM && (seq >= base || seq < (base + MAX_WINDOW_SIZE) % MAX_SEQ_NUM));
}

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

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst)
{
    static window_entry send_window[MAX_WINDOW_SIZE];
    int window_size = 1;

    struct timeval timeout, start, end;
    int addrlen = sizeof(struct sockaddr_in);
    pkt ack;

    int total_packets = (buf_len + MMS - 1) / MMS;
    int sent_packets = 0;
    int confirmed_packets = 0;
    int envios_simultaneos = 0;
    int acked_count = 0;

    float sample_rtt;

    while (confirmed_packets < total_packets)
    {
        while ((next_seqnum - snd_base + MAX_SEQ_NUM) % MAX_SEQ_NUM < window_size && sent_packets < total_packets)
        {
            int offset = sent_packets * MMS;
            int chunk_size = (offset + MMS > buf_len) ? buf_len - offset : MMS;

            make_pkt(&send_window[next_seqnum % MAX_WINDOW_SIZE].packet, PKT_DATA, next_seqnum, (char *)buf + offset, chunk_size);

            send_window[next_seqnum % MAX_WINDOW_SIZE].acked = 0;

            gettimeofday(&send_window[next_seqnum % MAX_WINDOW_SIZE].send_time, NULL);

            sendto(sockfd, &send_window[next_seqnum % MAX_WINDOW_SIZE].packet,
                   send_window[next_seqnum % MAX_WINDOW_SIZE].packet.h.pkt_size,
                   MSG_CONFIRM, (struct sockaddr *)dst, addrlen);

            printf("Sent packet SeqNum: %d\n", next_seqnum);

            next_seqnum = (next_seqnum + 1) % MAX_SEQ_NUM;
            sent_packets++;
            envios_simultaneos++;
            printf("Envios simultaneos: %d\n", envios_simultaneos);
        }

        settimer(sockfd, timeout, msec);

        int nr = recvfrom(sockfd, &ack, sizeof(pkt), MSG_WAITALL,
                          (struct sockaddr *)dst, (socklen_t *)&addrlen);

        if (nr > 0 && !iscorrupted(&ack) && ack.h.pkt_type == PKT_ACK)
        {
            int ack_seq = ack.h.pkt_seq;
            if (in_window(ack_seq, snd_base) && !send_window[ack_seq % MAX_WINDOW_SIZE].acked)
            {
                printf("ACK: %d recebido com sucesso!\n", ack_seq);

                send_window[ack_seq % MAX_WINDOW_SIZE].acked = 1;
                confirmed_packets++;
                acked_count++;
                envios_simultaneos--;

                start = send_window[ack_seq % MAX_WINDOW_SIZE].send_time;
                gettimeofday(&end, NULL);

                sample_rtt = time_diff(&start, &end);
                msec = update_timeout(sample_rtt);

                printf("rtt: %.3f\n", sample_rtt);
                printf("Novo tempo limite: %.3f\n", msec);

                while (send_window[snd_base % MAX_WINDOW_SIZE].acked)
                {
                    send_window[snd_base % MAX_WINDOW_SIZE].acked = 0;
                    snd_base = (snd_base + 1) % MAX_SEQ_NUM;
                }
                if (window_size < MAX_WINDOW_SIZE && acked_count >= window_size)
                {
                    acked_count = 0;
                    window_size++;
                    printf("Tamanho da janela: %d\n", window_size);
                }
            }
        }
        else
        {
            printf("Estouro de timer\n");
            gettimeofday(&end, NULL);
            for (int i = snd_base; i != next_seqnum; i = (i + 1) % MAX_SEQ_NUM)
            {
                int idx = i % MAX_WINDOW_SIZE;
                float elapsed_time = time_diff(&send_window[idx].send_time, &end);
                if (!send_window[idx].acked && elapsed_time > msec)
                {
                    acked_count = 0;
                    window_size = window_size / 2;
                    printf("Tamanho da janela resetado: %d\n", window_size);

                    gettimeofday(&send_window[idx].send_time, NULL);
                    sendto(sockfd, &send_window[idx].packet, send_window[idx].packet.h.pkt_size,
                           MSG_CONFIRM, (struct sockaddr *)dst, addrlen);
                    printf("Retransmitting packet SeqNum: %d\n", send_window[idx].packet.h.pkt_seq);
                }
            }
        }
        printf("Pacotes confirmados: %d\n", confirmed_packets);
        fflush(stdout);
    }
    return 0;
}

int has_dataseqnum(pkt *p, hseq_t seqnum)
{
    return (p->h.pkt_seq == seqnum && p->h.pkt_type == PKT_DATA);
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src)
{
    static pkt recv_window[MAX_WINDOW_SIZE];
    static int received[MAX_WINDOW_SIZE] = {0};
    pkt p, ack;
    int nr, addrlen = sizeof(struct sockaddr_in);
    int received_count = 0;
    int received_in_order = 0;
    int expected_packets = (buf_len + MMS - 1) / MMS;
    int offset = 0;

    while (received_count < expected_packets)
    {
        nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr *)src, (socklen_t *)&addrlen);

        if (nr > 0 && !iscorrupted(&p))
        {
            int seqnum = p.h.pkt_seq;

            if (in_window(seqnum, rcv_base))
            {
                int index = seqnum % MAX_WINDOW_SIZE;
                if (!received[index])
                {
                    recv_window[index] = p;
                    received[index] = 1;
                    received_count++;
                }
                while (received[rcv_base % MAX_WINDOW_SIZE])
                {
                    memcpy(buf + offset, recv_window[rcv_base % MAX_WINDOW_SIZE].msg, recv_window[rcv_base % MAX_WINDOW_SIZE].h.pkt_size - sizeof(hdr));
                    received[rcv_base % MAX_WINDOW_SIZE] = 0;
                    rcv_base = (rcv_base + 1) % MAX_SEQ_NUM;
                    received_in_order++;
                    offset += (p.h.pkt_size - sizeof(hdr));
                }
            }
            make_pkt(&ack, PKT_ACK, seqnum, NULL, 0);

            printf("ACK: %d\n", seqnum);
            fflush(stdout);

            sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, addrlen);
        }
    }
    return 0;
}
