/* ipc_shm.c — Pessoa 2: paradigma 1 (shm + sem) */
#include "common.h"
#include <errno.h>

/* TODO Pessoa 2:
 * - p segmentos shmget(IPC_PRIVATE, SHM_SIZE, ...) + shmat no init (filhos herdam)
 * - 1 semget(IPC_PRIVATE, 3*p, ...) inicializado com SEM_MUTEX/VAZIO/CHEIO
 * - put: P(VAZIO) P(MUTEX) slots[tail] ... V(MUTEX) V(CHEIO)
 * - get: P(CHEIO) [IPC_NOWAIT se !bloqueante -> EAGAIN] P(MUTEX) ... V(MUTEX) V(VAZIO)
 * - tratar EINTR no semop (repetir)
 * - destroy: shmdt + shmctl(IPC_RMID) em todos, semctl(IPC_RMID)
 */

int shmbuf_init(int p)                              { (void)p; errno = ENOSYS; return -1; }
int shmbuf_put(int fila, const msg_t *m)            { (void)fila; (void)m; errno = ENOSYS; return -1; }
int shmbuf_get(int fila, msg_t *m, int bloqueante)  { (void)fila; (void)m; (void)bloqueante; errno = ENOSYS; return -1; }
void shmbuf_destroy(void)                           { }
