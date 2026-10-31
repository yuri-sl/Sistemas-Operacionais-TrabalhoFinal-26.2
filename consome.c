/* consome.c — Pessoa 5: processo consumidor (+ testes) */
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

void ack_confirmar(const msg_t *m)
{
    if (m != NULL && m->ack == ACK_SIM) {
        union sigval v;
        v.sival_int = m->seq;
        if (sigqueue(m->pid, SIG_ACK, v) == -1) {
            /* Se o produtor já terminou/morreu (ESRCH), ignora o erro */
            if (errno != ESRCH) {
                perror("[consome] Falha ao enviar SIG_ACK via sigqueue");
            }
        }
    }
}

void consumo_notificar(int fila)
{
    union sigval v;
    v.sival_int = fila;
    if (sigqueue(g_mpmc_pid, SIG_CONSUMO, v) == -1) {
        perror("[consome] Falha ao enviar SIG_CONSUMO via sigqueue");
    }
}

void consome_main(int fila, int n_msgs, int ack)
{
    int consumidas = 0;
    int bloqueante = (ack == ACK_SIM) ? 1 : 0;
    msg_t m;

    for (int i = 0; i < n_msgs; i++) {
        int ret = ipc_get(fila, &m, bloqueante);

        if (ret == -1) {
            if (!bloqueante && errno == EAGAIN) {
                /* erro informando quantas foram consumidas */
                fprintf(stderr, "erro: consumidas %d de %d\n", consumidas, n_msgs);
                _exit(1);
            }
            /* Trata interrupção por sinal repetindo a leitura */
            if (errno == EINTR) {
                i--;
                continue;
            }
            perror("[consome] Erro inesperado em ipc_get");
            _exit(1);
        }

        consumidas++;

        /* 1. Confirma o recebimento ao produtor */
        ack_confirmar(&m);

        /* 2. Notifica o shell mpmc do consumo efetuado */
        consumo_notificar(fila);
    }

    _exit(0);
}