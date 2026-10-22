/* consome.c — Pessoa 5: processo consumidor (+ testes) */
#include "common.h"
#include <unistd.h>

/* TODO Pessoa 5:
 * consome_main:
 *  for i in 1..n_msgs:
 *     ack == ACK_SIM -> ipc_get(fila, &m, 1)   (bloqueia até chegar)
 *     ack == ACK_NAO -> ipc_get(fila, &m, 0)   (EAGAIN -> para)
 *     depois de cada get OK: ack_confirmar(&m); consumo_notificar(fila);
 *  completo -> _exit(0)
 *  parcial  -> imprimir "erro: consumidas k de n", _exit(1)
 *
 * ack_confirmar:     se m->ack == ACK_SIM -> sigqueue(m->pid, SIG_ACK, v)
 *                    (ESRCH = produtor já morreu -> ignorar)
 * consumo_notificar: sigqueue(g_mpmc_pid, SIG_CONSUMO, (union sigval){ .sival_int = fila })
 */

void ack_confirmar(const msg_t *m)   { (void)m; }
void consumo_notificar(int fila)     { (void)fila; }

void consome_main(int fila, int n_msgs, int ack)
{
    (void)fila; (void)n_msgs; (void)ack;
    _exit(0);
}
