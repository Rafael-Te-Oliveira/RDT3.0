#include "rdt3.0.h"

float estimated_rtt = 100; // Tempo inicial em ms
float dev_rtt = 0;
float msec = 100;

static hseq_t snd_base = 0; // Base da janela do emissor
static hseq_t next_seqnum = 0;
static hseq_t rcv_base = 0; // Base da janela do receptor

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
        sum += *(unsigned short *)buf;
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

void corrupt_packet(pkt *p)
{
    int packet_size = p->h.pkt_size;

    // Escolhe um byte aleatório dentro do pacote para corromper
    int byte_index = rand() % packet_size;
    unsigned char *data = (unsigned char *)p;

    // Escolhe um bit aleatório dentro do byte escolhido
    unsigned char bit_mask = 1 << (rand() % 8);

    // Inverte o bit escolhido para corromper o pacote
    data[byte_index] ^= bit_mask;
}

void delay_ack()
{
    int delay = (rand() % 10);
    usleep(delay * 30000);
}

int make_pkt(pkt *p, htype_t type, hseq_t seqnum, void *msg, int msg_len)
{

    if (msg_len > MAX_MSG_LEN)
    {
        // implementar fragmentação
    }
    // return ERROR;
    p->h.pkt_size = sizeof(hdr);
    p->h.csum = 0;
    p->h.pkt_type = type;
    p->h.pkt_seq = seqnum;
    if (msg_len > 0)
    {
        p->h.pkt_size += msg_len;
        memset(p->msg, 0, MAX_MSG_LEN);
        memcpy(p->msg, msg, msg_len);
    }
    p->h.csum = checksum((unsigned short *)p, p->h.pkt_size);
    return SUCCESS;
}

int has_ackseq(pkt *p, hseq_t seqnum)
{
    // implementar janela de seqnum
    return (p->h.pkt_type == PKT_ACK && p->h.pkt_seq == seqnum);
}

int rdt_send(int sockfd, void *buf, int buf_len, struct sockaddr_in *dst)
{
    pkt p, ack;
    struct sockaddr_in dst_ack;
    struct timeval timeout, start, end;
    int ns, nr, addrlen;

    float sample_rtt;
    int attempts = 1;

    make_pkt(&p, PKT_DATA, next_seqnum, buf, buf_len);

    do
    {

        gettimeofday(&start, NULL);
        ns = sendto(sockfd, &p, p.h.pkt_size, MSG_CONFIRM, (struct sockaddr *)dst, sizeof(struct sockaddr_in));
        if (ns < 0)
            return ERROR;

        settimer(sockfd, timeout, msec);
        printf("Tried to send packet SeqNum:%d %d times!\n", p.h.pkt_seq, attempts);
        fflush(stdout);
        attempts++;

        addrlen = sizeof(struct sockaddr_in);
        nr = recvfrom(sockfd, &ack, sizeof(ack), MSG_WAITALL, (struct sockaddr *)&dst_ack, (socklen_t *)&addrlen);

        if (nr < 0)
        {
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                perror("socket timeout");
                continue;
            }
        }
    } while (nr < 0 || iscorrupted(&ack) || !has_ackseq(&ack, next_seqnum));

    gettimeofday(&end, NULL);
    sample_rtt = time_diff(&start, &end);
    msec = update_timeout(sample_rtt);
    snd_base++;
    next_seqnum++;

    printf("%d bytes from %s: rtt=%.3f, SeqNum: %d\n",
           nr,
           inet_ntoa(dst->sin_addr),
           sample_rtt,
           p.h.pkt_seq);
    fflush(stdout);
    return 0;
}

int has_dataseqnum(pkt *p, hseq_t seqnum)
{
    return (p->h.pkt_seq == seqnum && p->h.pkt_type == PKT_DATA);
}

int rdt_recv(int sockfd, void *buf, int buf_len, struct sockaddr_in *src)
{
    pkt p, ack;
    int nr, ns, addrlen;

    addrlen = sizeof(struct sockaddr_in);

    do
    {
        nr = recvfrom(sockfd, &p, sizeof(pkt), 0, (struct sockaddr *)src, (socklen_t *)&addrlen);
    } while (nr < 0);

    if (iscorrupted(&p) || !has_dataseqnum(&p, rcv_base))
    {
        make_pkt(&ack, PKT_ACK, rcv_base - 1, NULL, 0);
    }
    else
    {
        memcpy(buf, p.msg, p.h.pkt_size - sizeof(hdr));
        make_pkt(&ack, PKT_ACK, p.h.pkt_seq, NULL, 0);
        rcv_base++;
    }

    delay_ack();

    ns = sendto(sockfd, &ack, ack.h.pkt_size, 0, (struct sockaddr *)src, sizeof(struct sockaddr_in));

    if (ns < 0)
        return ERROR;

    printf("%d bytes to %s: Ack: %d\n",
           ns,
           inet_ntoa(src->sin_addr),
           ack.h.pkt_seq);
    fflush(stdout);

    return 0;
}
