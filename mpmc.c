/* mpmc.c — Pessoa 1: shell, gerência de processos, shutdown */
#include "common.h"
#include <stdio.h>

int   g_paradigma;
int   g_p;
pid_t g_mpmc_pid;

/* Contador por fila, incrementado no handler de SIG_CONSUMO */
static volatile sig_atomic_t g_consumidas[MAX_PRODUTORES];

int main(int argc, char **argv)
{
    (void)argc; (void)argv; (void)g_consumidas;
    /* TODO Pessoa 1:
     * 1. Validar "mpmc p paradigma" (1 <= p <= MAX_PRODUTORES, paradigma 1|2).
     * 2. g_mpmc_pid = getpid();
     * 3. Instalar handler de SIG_CONSUMO (SA_SIGINFO | SA_RESTART):
     *      g_consumidas[si->si_value.sival_int]++
     *    -> ANTES do primeiro fork.
     * 4. ipc_init(paradigma, p)  -> ANTES do primeiro fork.
     * 5. Loop ">> ": produz / consome / shutdown.
     *    - converter fila 1..p -> 0..p-1; produz 0 -> FILA_STRIPED
     *    - validar n_msgs (1..1024), ack (1|2), fila existente
     *      ex.: "produz 3 2 1" com p=2 -> "erro - fila 3 nao existe"
     *    - fork(); filho chama produz_main/consome_main
     *    - tabela de filhos: pid, tipo, t_inicio, t_fim
     *    - colher filhos que terminaram (waitpid WNOHANG) e anotar t_fim
     * 6. shutdown: kill(SIGTERM) nos vivos, waitpid em todos, imprimir tempo
     *    de cada filho, g_consumidas por fila, tempo do mpmc, ipc_destroy().
     */
    return 0;
}
