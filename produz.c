/* produz.c: processo produtor */
#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static sigset_t g_so_ack;

/* Bloqueia o SIG_ACK para ele ficar pendente até o sigwaitinfo, em vez de
 * matar o processo (ação padrão de um sinal de tempo real). Precisa vir antes
 * do primeiro envio: um consumidor rápido pode confirmar enquanto o produtor
 * ainda envia. */
void ack_preparar(void)
{
    sigemptyset(&g_so_ack);
    sigaddset(&g_so_ack, SIG_ACK);
    sigprocmask(SIG_BLOCK, &g_so_ack, NULL);
}

int ack_esperar(int n)
{
    for (int recebidos = 0; recebidos < n; ) {
        if (sigwaitinfo(&g_so_ack, NULL) != -1)
            recebidos++;
        else if (errno != EINTR)
            return -1;
    }
    return 0;
}

static msg_t nova_msg(pid_t meu_pid, int seq, int ack)
{
    msg_t m;
    memset(&m, 0, sizeof m);
    m.pid = meu_pid;
    m.seq = seq;
    m.ack = ack;
    clock_gettime(CLOCK_REALTIME, &m.t_envio);
    return m;
}

/* No striped, seq 1 vai para a fila 0, seq 2 para a fila 1, e assim por diante. */
static int fila_destino(int fila, int seq)
{
    return fila == FILA_STRIPED ? (seq - 1) % g_p : fila;
}

static _Noreturn void falhar(const char *o_que, int seq)
{
    fprintf(stderr, "produz[%d]: erro %s (msg %d): %s\n",
            (int)getpid(), o_que, seq, strerror(errno));
    _exit(1);
}

void produz_main(int fila, int n_msgs, int ack)
{
    ack_preparar();
    pid_t meu_pid = getpid();

    for (int seq = 1; seq <= n_msgs; seq++) {
        msg_t m = nova_msg(meu_pid, seq, ack);
        if (ipc_put(fila_destino(fila, seq), &m) == -1)
            falhar("ao enviar", seq);
    }

    if (ack == ACK_SIM && ack_esperar(n_msgs) == -1)
        falhar("ao esperar as confirmações", n_msgs);

    _exit(0);
}
