/* produz.c — Pessoa 4: processo produtor
 *
 * Roteiro do produtor (filho do mpmc, criado por fork):
 *   1. liga o "não perturbe" do SIG_ACK      -> ack_preparar()
 *   2. envia as n mensagens, uma a uma        -> nova_msg() + enviar()
 *   3. se ack = 1, espera n confirmações      -> ack_esperar()
 *   4. sai                                    -> _exit()
 *
 */
#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* Conjunto com um único sinal, o SIG_ACK. */
static sigset_t g_so_ack;

/* ------------------------------------------------------------ confirmações */

/* Bloqueia o SIG_ACK. Bloqueado, o sinal não mata o processo (ação padrão
 * de um sinal de tempo real): fica guardado até ack_esperar() retirá-lo.
 * Tem de vir ANTES do primeiro envio, porque um consumidor rápido pode
 * confirmar a mensagem 1 enquanto ainda estamos enviando a 2. */
void ack_preparar(void)
{
    sigemptyset(&g_so_ack);
    sigaddset(&g_so_ack, SIG_ACK);
    sigprocmask(SIG_BLOCK, &g_so_ack, NULL);
}

/* Retira n SIG_ACK guardados, dormindo enquanto faltar algum.
 * Retorno: 0 = ok, -1 = erro (errno setado). */
int ack_esperar(int n)
{
    for (int recebidos = 0; recebidos < n; ) {
        if (sigwaitinfo(&g_so_ack, NULL) != -1)
            recebidos++;
        else if (errno != EINTR)        /* EINTR: outro sinal; tenta de novo */
            return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------- envio */

/* Monta a "etiqueta" da mensagem. */
static msg_t nova_msg(pid_t meu_pid, int seq, int ack)
{
    msg_t m;
    memset(&m, 0, sizeof m);            /* zera também os bytes de padding */
    m.pid = meu_pid;
    m.seq = seq;
    m.ack = ack;
    clock_gettime(CLOCK_REALTIME, &m.t_envio);
    return m;
}

/* Fila fixa, ou rodízio no modo striped: seq 1 -> fila 0, seq 2 -> fila 1... */
static int fila_destino(int fila, int seq)
{
    return fila == FILA_STRIPED ? (seq - 1) % g_p : fila;
}

/* ipc_put repetindo se um sinal interromper (defensivo; o backend já trata). */
static int enviar(int fila, const msg_t *m)
{
    int r;
    do {
        r = ipc_put(fila, m);
    } while (r == -1 && errno == EINTR);
    return r;
}

/* Erro fatal. Usa stderr porque ele não tem buffer: com _exit(), um printf
 * em stdout poderia se perder. */
static _Noreturn void falhar(const char *o_que, int seq)
{
    fprintf(stderr, "produz[%d]: erro %s (msg %d): %s\n",
            (int)getpid(), o_que, seq, strerror(errno));
    _exit(1);
}

/* -------------------------------------------------------------- principal */

void produz_main(int fila, int n_msgs, int ack)
{
    ack_preparar();                     /* SEMPRE a primeira coisa */
    pid_t meu_pid = getpid();

    for (int seq = 1; seq <= n_msgs; seq++) {
        msg_t m       = nova_msg(meu_pid, seq, ack);
        int   destino = fila_destino(fila, seq);

        if (enviar(destino, &m) == -1)
            falhar("ao enviar", seq);
    }

    if (ack == ACK_SIM && ack_esperar(n_msgs) == -1)
        falhar("ao esperar as confirmações", n_msgs);

    _exit(0);
}