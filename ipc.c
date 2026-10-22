/* ipc.c — despacho para o paradigma escolhido. Pronto; não precisa mexer. */
#include "common.h"
#include <errno.h>

static int fila_valida(int fila)
{
    if (fila < 0 || fila >= g_p) {
        errno = EINVAL;
        return 0;
    }
    return 1;
}

int ipc_init(int paradigma, int p)
{
    g_paradigma = paradigma;
    g_p = p;
    return paradigma == PARADIGMA_SHM ? shmbuf_init(p) : msgq_init(p);
}

int ipc_put(int fila, const msg_t *m)
{
    if (!fila_valida(fila)) return -1;
    return g_paradigma == PARADIGMA_SHM ? shmbuf_put(fila, m) : msgq_put(fila, m);
}

int ipc_get(int fila, msg_t *m, int bloqueante)
{
    if (!fila_valida(fila)) return -1;
    return g_paradigma == PARADIGMA_SHM ? shmbuf_get(fila, m, bloqueante)
                                        : msgq_get(fila, m, bloqueante);
}

void ipc_destroy(void)
{
    if (g_paradigma == PARADIGMA_SHM) shmbuf_destroy();
    else                              msgq_destroy();
}
