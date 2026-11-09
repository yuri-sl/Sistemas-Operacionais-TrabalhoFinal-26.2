/* consome.c: processo consumidor */
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void ack_confirmar(const msg_t *m)
{
    if (m->ack != ACK_SIM)
        return;

    union sigval v;
    v.sival_int = m->seq;
    /* ESRCH: o produtor já terminou, não há a quem confirmar. */
    if (sigqueue(m->pid, SIG_ACK, v) == -1 && errno != ESRCH)
        perror("consome: sigqueue(SIG_ACK)");
}

void consumo_notificar(int fila)
{
    union sigval v;
    v.sival_int = fila;
    if (sigqueue(g_mpmc_pid, SIG_CONSUMO, v) == -1)
        perror("consome: sigqueue(SIG_CONSUMO)");
}

/* ack = 1: espera a fila ter mensagens; ack = 2: desiste se a fila estiver vazia. */
void consome_main(int fila, int n_msgs, int ack)
{
    int bloqueante = (ack == ACK_SIM);
    int consumidas = 0;
    msg_t m;

    while (consumidas < n_msgs) {
        if (ipc_get(fila, &m, bloqueante) == -1) {
            if (errno == EINTR)
                continue;
            if (errno == EAGAIN)
                fprintf(stderr, "erro: consumidas %d de %d\n", consumidas, n_msgs);
            else
                perror("consome: ipc_get");
            _exit(1);
        }

        consumidas++;
        ack_confirmar(&m);
        consumo_notificar(fila);
    }

    _exit(0);
}
