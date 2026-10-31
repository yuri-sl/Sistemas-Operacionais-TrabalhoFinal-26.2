/* ipc_shm.c — Pessoa 2: paradigma 1 (shm + sem) */
#include "common.h"
#include <errno.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

/* A glibc não declara esta union: quem chama semctl é que define */
union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

/* Preenchidos no init, antes do 1º fork: os filhos herdam ids e mapeamentos */
static int        g_shmid[MAX_PRODUTORES];
static shm_buf_t *g_buf[MAX_PRODUTORES];
static int        g_nsegs;                  /* segmentos criados em g_shmid */
static int        g_semid = -1;

/* Repete em EINTR; os demais erros (EAGAIN, EIDRM...) vão para o chamador */
static int sem_alterar(int sem, int delta, int flags)
{
    struct sembuf op = { .sem_num = sem, .sem_op = delta, .sem_flg = flags };
    int r;

    do {
        r = semop(g_semid, &op, 1);
    } while (r == -1 && errno == EINTR);
    return r;
}

static int P(int sem, int flags) { return sem_alterar(sem, -1, flags); }
static int V(int sem)            { return sem_alterar(sem, +1, 0); }

int shmbuf_init(int p)
{
    unsigned short valores[3 * MAX_PRODUTORES];
    union semun arg;
    int erro;

    if (p < 1 || p > MAX_PRODUTORES) {
        errno = EINVAL;
        return -1;
    }

    for (int i = 0; i < p; i++) {
        g_shmid[i] = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600);
        if (g_shmid[i] == -1)
            goto falha;
        g_nsegs = i + 1;

        g_buf[i] = shmat(g_shmid[i], NULL, 0);
        if (g_buf[i] == (void *)-1) {
            g_buf[i] = NULL;
            goto falha;
        }
        g_buf[i]->hdr.head = 0;
        g_buf[i]->hdr.tail = 0;

        valores[SEM_MUTEX(i)] = 1;
        valores[SEM_VAZIO(i)] = SHM_SLOTS;
        valores[SEM_CHEIO(i)] = 0;
    }

    g_semid = semget(IPC_PRIVATE, 3 * p, IPC_CREAT | 0600);
    if (g_semid == -1)
        goto falha;

    arg.array = valores;
    if (semctl(g_semid, 0, SETALL, arg) == -1)
        goto falha;
    return 0;

falha:
    erro = errno;
    shmbuf_destroy();
    errno = erro;
    return -1;
}

int shmbuf_put(int fila, const msg_t *m)
{
    shm_buf_t *b = g_buf[fila];

    if (P(SEM_VAZIO(fila), 0) == -1 || P(SEM_MUTEX(fila), 0) == -1)
        return -1;

    b->slots[b->hdr.tail] = *m;
    b->hdr.tail = (b->hdr.tail + 1) % SHM_SLOTS;

    if (V(SEM_MUTEX(fila)) == -1 || V(SEM_CHEIO(fila)) == -1)
        return -1;
    return 0;
}

int shmbuf_get(int fila, msg_t *m, int bloqueante)
{
    shm_buf_t *b = g_buf[fila];

    /* Só o CHEIO depende do modo. O MUTEX bloqueia sempre: com IPC_NOWAIT,
     * uma simples disputa pelo mutex seria confundida com fila vazia. */
    if (P(SEM_CHEIO(fila), bloqueante ? 0 : IPC_NOWAIT) == -1 ||
        P(SEM_MUTEX(fila), 0) == -1)
        return -1;

    *m = b->slots[b->hdr.head];
    b->hdr.head = (b->hdr.head + 1) % SHM_SLOTS;

    if (V(SEM_MUTEX(fila)) == -1 || V(SEM_VAZIO(fila)) == -1)
        return -1;
    return 0;
}

/* Também desfaz um init incompleto, por isso confere o que chegou a existir */
void shmbuf_destroy(void)
{
    for (int i = 0; i < g_nsegs; i++) {
        if (g_buf[i] != NULL) {
            shmdt(g_buf[i]);
            g_buf[i] = NULL;
        }
        shmctl(g_shmid[i], IPC_RMID, NULL);
    }
    g_nsegs = 0;

    if (g_semid != -1) {
        semctl(g_semid, 0, IPC_RMID);
        g_semid = -1;
    }
}
