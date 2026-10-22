/* produz.c — Pessoa 4: processo produtor */
#include "common.h"
#include <unistd.h>

/* TODO Pessoa 4:
 * produz_main:
 *  1. ack_preparar() SEMPRE como primeira coisa (a ação padrão de um sinal
 *     RT é matar o processo; se um ack chegar antes do bloqueio, morremos).
 *  2. Para seq = 1..n_msgs: monta msg_t {getpid(), seq, ack, t_envio REALTIME}
 *     fila destino = fila, ou (seq-1) % g_p se fila == FILA_STRIPED
 *     ipc_put(destino, &m)
 *  3. Se ack == ACK_SIM: ack_esperar(n_msgs)
 *  4. _exit(0)
 *
 * Ex. striped: "produz 0 15 2" com p = 5 -> seq 1,6,11 na fila 0; 2,7,12 na fila 1; ...
 */

void ack_preparar(void)              { }
int  ack_esperar(int n)              { (void)n; return 0; }

void produz_main(int fila, int n_msgs, int ack)
{
    (void)fila; (void)n_msgs; (void)ack;
    _exit(0);
}
