/* ipc_msg.c — Pessoa 3: paradigma 2 (fila de mensagens System V) */
#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>

/*
 * Estado privado do módulo.
 *
 * g_ids guarda os identificadores das p filas criadas por msgq_init().
 * O sentinela é -1, e não 0: zero pode ser um identificador de fila válido
 * devolvido por msgget().
 *
 * Os filhos criados com fork() herdam uma cópia deste vetor (e de g_msgq_p).
 * O identificador é apenas um número que referencia um objeto mantido pelo
 * kernel; por isso a cópia herdada aponta para a mesma fila do processo pai.
 */
#define MSGQ_ID_INVAL (-1)

static int *g_ids;
static int  g_msgq_p;

/* Confere se a fila está no intervalo 0..g_msgq_p-1 e se o módulo foi iniciado. */
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
    int i;

    /* g_ids != NULL: já inicializado; recusar evita vazar as filas antigas. */
    if (p < 1 || p > MAX_PRODUTORES || g_ids != NULL) {
        errno = EINVAL;
        return -1;
    }

    ids = malloc((size_t)p * sizeof(*ids));
    if (ids == NULL)
        return -1;                  /* malloc já definiu errno (ENOMEM) */

    for (i = 0; i < p; i++)
        ids[i] = MSGQ_ID_INVAL;

    for (i = 0; i < p; i++) {
        ids[i] = msgget(IPC_PRIVATE, 0600);
        if (ids[i] == -1) {
            int erro = errno;
            int j;

            /* Remove apenas as filas criadas nesta tentativa, sem tocar em
             * recursos de outros programas. */
            for (j = 0; j < i; j++) {
                if (ids[j] != MSGQ_ID_INVAL)
                    (void)msgctl(ids[j], IPC_RMID, NULL);
            }
            free(ids);
            errno = erro;       /* preserva o erro original da criação */
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

    /* Copia integral: pid, seq, ack e t_envio são responsabilidade do
     * produtor; o backend não altera nenhum campo. */
    envio.mtype = MQ_MTYPE;
    envio.m = *m;

    for (;;) {
        /* flags = 0: bloqueia quando a fila está cheia (msgmnb). */
        if (msgsnd(g_ids[fila], &envio, sizeof(msg_t), 0) == 0)
            return 0;
        if (errno != EINTR)
            return -1;
        /* Um sinal interrompeu a espera por espaço: repete a chamada. */
    }
}

int msgq_get(int fila, msg_t *m, int bloqueante)
{
    mq_msg_t recebido;
    int flags = bloqueante ? 0 : IPC_NOWAIT;

    if (!fila_valida(fila) || m == NULL) {
        errno = EINVAL;
        return -1;
    }

    for (;;) {
        if (msgrcv(g_ids[fila], &recebido, sizeof(msg_t), MQ_MTYPE, flags)
            != -1) {
            /* Só copia para o chamador depois de um recebimento bem-sucedido. */
            *m = recebido.m;
            return 0;
        }

        if (errno == EINTR)
            continue;               /* sinal interrompeu: repete a chamada */

        /* Contrato: fila vazia em modo não bloqueante devolve EAGAIN. */
        if (!bloqueante && errno == ENOMSG)
            errno = EAGAIN;

        return -1;
    }
}

void msgq_destroy(void)
{
    int i;

    if (g_ids == NULL)
        return;                     /* chamada repetida é inofensiva */

    for (i = 0; i < g_msgq_p; i++) {
        if (g_ids[i] != MSGQ_ID_INVAL) {
            /* Se uma remoção falhar (ex.: EIDRM), continua com as demais. */
            (void)msgctl(g_ids[i], IPC_RMID, NULL);
            g_ids[i] = MSGQ_ID_INVAL;
        }
    }

    free(g_ids);
    g_ids = NULL;
    g_msgq_p = 0;
}
