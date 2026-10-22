/*
 * common.h — CONTRATO do trabalho MPMC (SO 02/2026, Prof. Alba)
 *
 * Todo mundo inclui este arquivo. Qualquer mudança aqui -> avisar o grupo ANTES.
 *
 * Convenções gerais
 * -----------------
 * - Filas: o usuário digita 1..p; internamente TUDO é 0..p-1.
 *   Quem converte é o shell (mpmc.c). "produz 0 ..." vira fila = FILA_STRIPED.
 * - Todos os recursos IPC são criados com IPC_PRIVATE pelo mpmc, ANTES de
 *   qualquer fork. Os filhos herdam os ids (e o shmat) por fork. Sem ftok.
 * - Processos produz/consome são só fork() do mpmc (sem exec). O filho chama
 *   produz_main()/consome_main(), que terminam com _exit().
 * - Tempo de execução de cada filho: medido PELO MPMC (início no fork, fim no
 *   waitpid). Os filhos não precisam de handler de SIGTERM.
 * - Contagem de mensagens consumidas por fila: o consumidor manda SIG_CONSUMO
 *   ao mpmc a cada mensagem consumida (vale para os dois paradigmas).
 * - Ack produtor<-consumidor: o consumidor manda SIG_ACK ao pid que está na
 *   mensagem, só se m.ack == ACK_SIM.
 *   Usamos sinais de TEMPO REAL (sigqueue) porque eles ENFILEIRAM.
 *   SIGUSR1/SIGUSR2 NÃO servem: dois envios simultâneos viram um só e o
 *   produtor fica esperando para sempre.
 */
#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>
#include <signal.h>
#include <time.h>

/* ---------------------------------------------------------------- limites */
#define MAX_PRODUTORES 64
#define MIN_MSGS       1
#define MAX_MSGS       1024
#define SHM_SIZE       2048          /* exigido pelo enunciado */

#define PARADIGMA_SHM  1
#define PARADIGMA_MSG  2

#define ACK_SIM        1
#define ACK_NAO        2

#define FILA_STRIPED   (-1)

/* --------------------------------------------------------------- mensagem */
typedef struct {
    pid_t           pid;      /* pid do processo produz que enviou          */
    int             seq;      /* 1..n_msgs, sequencial POR processo emissor */
    int             ack;      /* ACK_SIM -> consumidor confirma ao produtor */
    struct timespec t_envio;  /* clock_gettime(CLOCK_REALTIME) no put       */
} msg_t;                      /* 32 bytes em x86_64                         */

/* ---------------------------------------- paradigma 1: shm + sem (Pessoa 2) */
typedef struct {
    int head;                 /* próximo slot a ler     */
    int tail;                 /* próximo slot a escrever */
} shm_hdr_t;

#define SHM_SLOTS ((SHM_SIZE - sizeof(shm_hdr_t)) / sizeof(msg_t))  /* = 63 */

typedef struct {
    shm_hdr_t hdr;
    msg_t     slots[SHM_SLOTS];
} shm_buf_t;

_Static_assert(sizeof(shm_buf_t) <= SHM_SIZE, "buffer nao cabe em 2048 bytes");

/* Um único conjunto de semáforos (semget) com 3*p semáforos.
 * Ex.: p = 4 -> 12 semáforos; fila 2 (interna) usa os índices 6, 7 e 8. */
#define SEM_MUTEX(i) (3 * (i))       /* init 1          */
#define SEM_VAZIO(i) (3 * (i) + 1)   /* init SHM_SLOTS  */
#define SEM_CHEIO(i) (3 * (i) + 2)   /* init 0          */

/* --------------------------------------- paradigma 2: msg queue (Pessoa 3) */
typedef struct {
    long  mtype;              /* sempre MQ_MTYPE */
    msg_t m;
} mq_msg_t;

#define MQ_MTYPE 1L
/* msgsnd(id, &x, sizeof(msg_t), 0)  -- o tamanho NÃO inclui o mtype */

/* ---------------------------------------------------------------- sinais */
#define SIG_ACK     (SIGRTMIN)       /* consumidor -> produtor (1 por msg)      */
#define SIG_CONSUMO (SIGRTMIN + 1)   /* consumidor -> mpmc, sival_int = fila    */

/* ------------------------------------------------- globais (em mpmc.c) */
extern int   g_paradigma;
extern int   g_p;
extern pid_t g_mpmc_pid;

/* ------------------------------------------- interface IPC (ipc.c) ------
 * Produz/consome usam SÓ estas 4 funções; não sabem qual é o paradigma.
 * Retorno: 0 = ok, -1 = erro (errno setado).
 *   ipc_get não bloqueante e fila vazia -> -1 com errno = EAGAIN.
 *   fila fora de 0..p-1                 -> -1 com errno = EINVAL.
 */
int  ipc_init(int paradigma, int p);               /* mpmc, antes do 1º fork */
int  ipc_put(int fila, const msg_t *m);            /* bloqueia se cheio       */
int  ipc_get(int fila, msg_t *m, int bloqueante);
void ipc_destroy(void);                            /* mpmc, no shutdown       */

/* Implementações de cada paradigma (ipc.c faz o despacho) */
int  shmbuf_init(int p);                           /* Pessoa 2: ipc_shm.c */
int  shmbuf_put(int fila, const msg_t *m);
int  shmbuf_get(int fila, msg_t *m, int bloqueante);
void shmbuf_destroy(void);

int  msgq_init(int p);                             /* Pessoa 3: ipc_msg.c */
int  msgq_put(int fila, const msg_t *m);
int  msgq_get(int fila, msg_t *m, int bloqueante);
void msgq_destroy(void);

/* ------------------------------------------------ produz.c (Pessoa 4) */
void produz_main(int fila, int n_msgs, int ack);   /* fila 0..p-1 ou FILA_STRIPED */
void ack_preparar(void);   /* bloqueia SIG_ACK; chamar ANTES do 1º ipc_put  */
int  ack_esperar(int n);   /* sigwaitinfo até receber n SIG_ACK             */

/* ------------------------------------------------ consome.c (Pessoa 5) */
void consome_main(int fila, int n_msgs, int ack);  /* _exit(0) completo, _exit(1) parcial */
void ack_confirmar(const msg_t *m);                /* se m->ack == ACK_SIM: sigqueue(m->pid, SIG_ACK) */
void consumo_notificar(int fila);                  /* sigqueue(g_mpmc_pid, SIG_CONSUMO, {fila}) */

/* --------------------------------------------------------------- utilitário */
static inline double tempo_seg(void)               /* para medir durações */
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

#endif /* COMMON_H */
