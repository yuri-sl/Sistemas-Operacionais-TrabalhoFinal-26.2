/* ipc_msg.c — Pessoa 3: paradigma 2 (msg queue) */
#include "common.h"
#include <errno.h>

/* TODO Pessoa 3:
 * - p filas msgget(IPC_PRIVATE, ...) no init (ids num vetor global; filhos herdam)
 * - put: mq_msg_t x = { MQ_MTYPE, *m }; msgsnd(id, &x, sizeof(msg_t), 0)
 * - get: msgrcv(id, &x, sizeof(msg_t), MQ_MTYPE, bloqueante ? 0 : IPC_NOWAIT)
 *        ENOMSG (fila vazia sem bloquear) -> devolver errno = EAGAIN
 * - tratar EINTR (repetir)
 * - destroy: msgctl(IPC_RMID) em todas
 * - Atenção: limite msgmnb (~16 KB) -> ~500 msgs de 32 B por fila; acima disso o put bloqueia
 */

int msgq_init(int p)                              { (void)p; errno = ENOSYS; return -1; }
int msgq_put(int fila, const msg_t *m)            { (void)fila; (void)m; errno = ENOSYS; return -1; }
int msgq_get(int fila, msg_t *m, int bloqueante)  { (void)fila; (void)m; (void)bloqueante; errno = ENOSYS; return -1; }
void msgq_destroy(void)                           { }
