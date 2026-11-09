/* ver_shm.c: anexa (só leitura) um segmento do mpmc e mostra as mensagens.
 * Compilar na raiz do projeto: gcc -D_GNU_SOURCE -I. -o /tmp/ver_shm testes/ver_shm.c
 * Usar: /tmp/ver_shm <shmid>   (shmid vem do "ipcs -m"). Nao altera nada:
 *       anexa o segmento com SHM_RDONLY. Ver GUIA_VERIFICACAO.md. */
#include "common.h"
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <time.h>
#include <sys/shm.h>

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "uso: %s shmid\n", argv[0]); return 1; }

    printf("sizeof(msg_t)     = %zu bytes\n", sizeof(msg_t));
    printf("  pid     offset %2zu, %zu bytes\n", offsetof(msg_t, pid), sizeof(pid_t));
    printf("  seq     offset %2zu, %zu bytes\n", offsetof(msg_t, seq), sizeof(int));
    printf("  ack     offset %2zu, %zu bytes\n", offsetof(msg_t, ack), sizeof(int));
    printf("  t_envio offset %2zu, %zu bytes\n", offsetof(msg_t, t_envio), sizeof(struct timespec));
    printf("sizeof(shm_buf_t) = %zu bytes (cabecalho %zu + %zu slots)\n",
           sizeof(shm_buf_t), sizeof(shm_hdr_t), (size_t)SHM_SLOTS);

    int id = atoi(argv[1]);
    struct shmid_ds ds;
    if (shmctl(id, IPC_STAT, &ds) == -1) { perror("shmctl"); return 1; }
    printf("segmento %d: %zu bytes, %lu processo(s) anexado(s)\n",
           id, (size_t)ds.shm_segsz, (unsigned long)ds.shm_nattch);

    shm_buf_t *b = shmat(id, NULL, SHM_RDONLY);
    if (b == (void *)-1) { perror("shmat"); return 1; }
    printf("head=%d tail=%d\n", b->hdr.head, b->hdr.tail);
    for (size_t i = 0; i < SHM_SLOTS; i++) {
        msg_t *m = &b->slots[i];
        if (m->pid == 0) continue;               /* slot nunca usado */
        char hora[16];
        strftime(hora, sizeof hora, "%H:%M:%S", localtime(&m->t_envio.tv_sec));
        printf("  slot %2zu: pid=%d seq=%d ack=%d enviada=%s.%06ld\n",
               i, m->pid, m->seq, m->ack, hora, m->t_envio.tv_nsec / 1000);
    }
    shmdt(b);
    return 0;
}
