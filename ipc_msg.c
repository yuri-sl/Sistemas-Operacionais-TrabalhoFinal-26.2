/* ipc_msg.c: paradigma 2, filas de mensagens System V */
#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>

static int *g_ids;
static int  g_msgq_p;

static int fila_valida(int fila)
{
    if (g_ids == NULL || fila < 0 || fila >= g_msgq_p) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

int msgq_init(int p)
{
    int *ids;

    if (p < 1 || p > MAX_PRODUTORES || g_ids != NULL) {
        errno = EINVAL;
        return -1;
    }

    ids = malloc((size_t)p * sizeof(*ids));
    if (ids == NULL)
        return -1;

    for (int i = 0; i < p; i++) {
        ids[i] = msgget(IPC_PRIVATE, 0600);
        if (ids[i] == -1) {
            int erro = errno;
            for (int j = 0; j < i; j++)
                msgctl(ids[j], IPC_RMID, NULL);
            free(ids);
            errno = erro;
            return -1;
        }
    }

    g_ids = ids;
    g_msgq_p = p;
    return 0;
}

int msgq_put(int fila, const msg_t *m)
{
    mq_msg_t envio;

    if (!fila_valida(fila) || m == NULL) {
        errno = EINVAL;
        return -1;
    }

    envio.mtype = MQ_MTYPE;
    envio.m = *m;

    /* O tamanho passado ao msgsnd não inclui o mtype. Bloqueia com a fila cheia. */
    while (msgsnd(g_ids[fila], &envio, sizeof(msg_t), 0) == -1) {
        if (errno != EINTR)
            return -1;
    }
    return 0;
}

int msgq_get(int fila, msg_t *m, int bloqueante)
{
    mq_msg_t recebido;
    int flags = bloqueante ? 0 : IPC_NOWAIT;

    if (!fila_valida(fila) || m == NULL) {
        errno = EINVAL;
        return -1;
    }

    while (msgrcv(g_ids[fila], &recebido, sizeof(msg_t), MQ_MTYPE, flags) == -1) {
        if (errno == EINTR)
            continue;
        if (errno == ENOMSG)        /* fila vazia sem bloquear: mesmo erro do shm */
            errno = EAGAIN;
        return -1;
    }
    *m = recebido.m;
    return 0;
}

void msgq_destroy(void)
{
    if (g_ids == NULL)
        return;

    for (int i = 0; i < g_msgq_p; i++)
        msgctl(g_ids[i], IPC_RMID, NULL);

    free(g_ids);
    g_ids = NULL;
    g_msgq_p = 0;
}
