#ifndef RDT3_H
#define RDT3_H

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#define MAX_MSG_LEN 1000
#define TIMEOUT_SEC 2 // Tempo de espera para retransmissão
#define MSG_NUM 100

#define WINDOW_SIZE 4                 // Tamanho da janela deslizante
#define MAX_SEQ_NUM (2 * WINDOW_SIZE) // Números de sequência cíclicos

#define ALPHA 0.125
#define BETA 0.25
#define INITIAL_TIMEOUT 1000 // 1 segundo

#define ERROR -1
#define TRUE 1
#define FALSE 0
#define SUCCESS 0

typedef uint16_t hsize_t;
typedef uint16_t hcsum_t;
typedef uint16_t hseq_t;
typedef uint8_t htype_t;

#define PKT_ACK 0
#define PKT_DATA 1

struct hdr
{
    hseq_t pkt_seq;
    hsize_t pkt_size;
    htype_t pkt_type;
    hcsum_t csum;
};

typedef struct hdr hdr;

struct pkt
{
    hdr h;
    unsigned char msg[MAX_MSG_LEN];
};
typedef struct pkt pkt;

unsigned short checksum(unsigned short *, int);
int iscorrupted(pkt *);
int make_pkt(pkt *, htype_t, hseq_t, void *, int);
int has_ackseq(pkt *, hseq_t);
int rdt_send(int, void *, int, struct sockaddr_in *);
int has_dataseqnum(pkt *, hseq_t);
int rdt_recv(int, void *, int, struct sockaddr_in *);

#endif