/*
 * Definições compartilhadas por todos os módulos.
 *
 * - O usuário numera as filas de 1 a p; internamente elas vão de 0 a p-1.
 *   "produz 0" vira FILA_STRIPED.
 * - O mpmc cria os recursos IPC (IPC_PRIVATE) antes do primeiro fork, e os
 *   filhos os herdam.
 * - Produtor e consumidor são filhos do mpmc criados por fork, sem exec.
 * - O ack e a contagem de consumidas usam sinais de tempo real, que
 *   enfileiram: com SIGUSR1, dois envios simultâneos virariam um só.
 */
#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <signal.h>
#include <time.h>

#define MAX_PRODUTORES 64
#define MIN_MSGS       1
#define MAX_MSGS       1024
#define SHM_SIZE       2048

#define PARADIGMA_SHM  1
#define PARADIGMA_MSG  2

#define ACK_SIM        1
#define ACK_NAO        2

#define FILA_STRIPED   (-1)

typedef struct {
    pid_t           pid;      /* produtor que enviou */
    int             seq;      /* 1..n_msgs */
    int             ack;
    struct timespec t_envio;
} msg_t;

/* Paradigma 1: buffer circular em memória compartilhada */
typedef struct {
    int head;                 /* próximo slot a ler */
    int tail;                 /* próximo slot a escrever */
} shm_hdr_t;

#define SHM_SLOTS ((SHM_SIZE - sizeof(shm_hdr_t)) / sizeof(msg_t))  /* 63 */

typedef struct {
    shm_hdr_t hdr;
    msg_t     slots[SHM_SLOTS];
} shm_buf_t;

_Static_assert(sizeof(shm_buf_t) <= SHM_SIZE, "buffer nao cabe em 2048 bytes");

/* Um conjunto com 3*p semáforos; a fila i usa os índices 3i, 3i+1 e 3i+2. */
#define SEM_MUTEX(i) (3 * (i))
#define SEM_VAZIO(i) (3 * (i) + 1)
#define SEM_CHEIO(i) (3 * (i) + 2)

/* Paradigma 2: fila de mensagens */
typedef struct {
    long  mtype;
    msg_t m;
} mq_msg_t;

#define MQ_MTYPE 1L

#define SIG_ACK     (SIGRTMIN)       /* consumidor -> produtor */
#define SIG_CONSUMO (SIGRTMIN + 1)   /* consumidor -> mpmc, sival_int = fila */

extern int   g_paradigma;
extern int   g_p;
extern pid_t g_mpmc_pid;

/* Interface usada por produtor e consumidor, independente do paradigma.
 * Retornam 0 ou -1 com errno; ipc_get não bloqueante com a fila vazia
 * devolve EAGAIN. */
int  ipc_init(int paradigma, int p);
int  ipc_put(int fila, const msg_t *m);
int  ipc_get(int fila, msg_t *m, int bloqueante);
void ipc_destroy(void);

int  shmbuf_init(int p);
int  shmbuf_put(int fila, const msg_t *m);
int  shmbuf_get(int fila, msg_t *m, int bloqueante);
void shmbuf_destroy(void);

int  msgq_init(int p);
int  msgq_put(int fila, const msg_t *m);
int  msgq_get(int fila, msg_t *m, int bloqueante);
void msgq_destroy(void);

void produz_main(int fila, int n_msgs, int ack);
void ack_preparar(void);
int  ack_esperar(int n);

void consome_main(int fila, int n_msgs, int ack);
void ack_confirmar(const msg_t *m);
void consumo_notificar(int fila);

static inline double tempo_seg(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

#endif
