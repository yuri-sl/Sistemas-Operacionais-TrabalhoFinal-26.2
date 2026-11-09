/* mpmc.c: shell, gerência dos processos filhos e shutdown
 *
 * Uso: mpmc p paradigma   (paradigma 1 = memória compartilhada, 2 = filas)
 * Comandos: produz p n_msgs ack | consome p n_msgs ack | shutdown
 *
 * O tempo de cada filho é medido pelo mpmc, do fork até o waitpid. Enquanto
 * espera um comando, o mpmc também acompanha o SIGCHLD (via signalfd) e colhe
 * o filho assim que ele termina, então nenhum fica zumbi e o tempo não inclui
 * a espera no prompt.
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>
#include <poll.h>
#include <sys/signalfd.h>

int   g_paradigma;
int   g_p;
pid_t g_mpmc_pid;

#define TIPO_PRODUTOR    0
#define TIPO_CONSUMIDOR  1
#define MAX_FILHOS       4096

typedef struct {
    pid_t  pid;
    int    tipo;
    int    fila;        /* 0..g_p-1 ou FILA_STRIPED */
    int    n_msgs;
    double t_inicio;
    double t_fim;
    int    vivo;        /* 1 até ser colhido pelo waitpid */
} filho_t;

static filho_t g_filhos[MAX_FILHOS];
static int     g_nfilhos;

/* Só o handler escreve aqui. */
static volatile sig_atomic_t g_consumidas[MAX_PRODUTORES];

/* Cada mensagem consumida chega como um SIG_CONSUMO com a fila em sival_int. */
static void on_consumo(int sig, siginfo_t *si, void *uc)
{
    (void)sig; (void)uc;
    int fila = si->si_value.sival_int;
    if (fila >= 0 && fila < MAX_PRODUTORES)
        g_consumidas[fila]++;
}

static void instalar_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = on_consumo;
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIG_CONSUMO, &sa, NULL) == -1) {
        perror("sigaction(SIG_CONSUMO)");
        exit(1);
    }
}

static int parse_int(const char *s, int *out)
{
    char *fim;
    errno = 0;
    long v = strtol(s, &fim, 10);
    if (errno != 0 || fim == s || *fim != '\0' || v < INT_MIN || v > INT_MAX)
        return -1;
    *out = (int)v;
    return 0;
}

/* Colhe, sem bloquear, os filhos que já terminaram e anota o fim. */
static void colher_filhos(void)
{
    pid_t pid;
    int   status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < g_nfilhos; i++) {
            if (g_filhos[i].vivo && g_filhos[i].pid == pid) {
                g_filhos[i].t_fim = tempo_seg();
                g_filhos[i].vivo  = 0;
                break;
            }
        }
    }
}

static void criar_filho(int tipo, int fila, int n_msgs, int ack)
{
    if (g_nfilhos >= MAX_FILHOS) {
        printf(">>>> erro - limite de %d processos atingido\n", MAX_FILHOS);
        return;
    }

    double t0  = tempo_seg();
    pid_t  pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }
    if (pid == 0) {
        if (tipo == TIPO_PRODUTOR) produz_main(fila, n_msgs, ack);
        else                       consome_main(fila, n_msgs, ack);
        _exit(0);
    }

    filho_t *f = &g_filhos[g_nfilhos++];
    f->pid      = pid;
    f->tipo     = tipo;
    f->fila     = fila;
    f->n_msgs   = n_msgs;
    f->t_inicio = t0;
    f->t_fim    = t0;
    f->vivo     = 1;
}

/* tok = { "produz" | "consome", p, n_msgs, ack }, já sem o '&' final. */
static void cmd_produz_consome(int tipo, char **tok, int ntok)
{
    const char *nome = (tipo == TIPO_PRODUTOR) ? "produz" : "consome";

    if (ntok != 4) {
        printf(">>>> erro - uso: %s p n_msgs ack\n", nome);
        return;
    }

    int p, n_msgs, ack;
    if (parse_int(tok[1], &p) || parse_int(tok[2], &n_msgs) ||
        parse_int(tok[3], &ack)) {
        printf(">>>> erro - argumentos devem ser numeros inteiros\n");
        return;
    }

    if (ack != ACK_SIM && ack != ACK_NAO) {
        printf(">>>> erro - ack deve ser 1 (sim) ou 2 (nao)\n");
        return;
    }
    if (n_msgs < MIN_MSGS || n_msgs > MAX_MSGS) {
        printf(">>>> erro - n_msgs deve estar entre %d e %d\n",
               MIN_MSGS, MAX_MSGS);
        return;
    }

    /* Só o produz aceita a fila 0 (striped). */
    int fila;
    if (tipo == TIPO_PRODUTOR && p == 0) {
        fila = FILA_STRIPED;
    } else if (p >= 1 && p <= g_p) {
        fila = p - 1;
    } else {
        printf(">>>> erro - fila %d nao existe\n", p);
        return;
    }

    criar_filho(tipo, fila, n_msgs, ack);
}

static void cmd_shutdown(double t_mpmc_inicio)
{
    /* Filhos ainda vivos estão bloqueados esperando ack, espaço ou mensagem. */
    for (int i = 0; i < g_nfilhos; i++)
        if (g_filhos[i].vivo)
            kill(g_filhos[i].pid, SIGTERM);

    for (int i = 0; i < g_nfilhos; i++) {
        if (!g_filhos[i].vivo)
            continue;
        int status;
        while (waitpid(g_filhos[i].pid, &status, 0) == -1 && errno == EINTR)
            ;
        g_filhos[i].t_fim = tempo_seg();
        g_filhos[i].vivo  = 0;
    }

    printf("\n========== shutdown ==========\n");
    printf("Processos criados: %d\n", g_nfilhos);
    for (int i = 0; i < g_nfilhos; i++) {
        filho_t *f = &g_filhos[i];
        const char *nome = (f->tipo == TIPO_PRODUTOR) ? "produz" : "consome";
        if (f->fila == FILA_STRIPED)
            printf("  [%d] %-7s pid=%-7d fila=striped  tempo=%.6f s\n",
                   i, nome, f->pid, f->t_fim - f->t_inicio);
        else
            printf("  [%d] %-7s pid=%-7d fila=%-7d  tempo=%.6f s\n",
                   i, nome, f->pid, f->fila + 1, f->t_fim - f->t_inicio);
    }

    printf("Mensagens consumidas por fila:\n");
    for (int i = 0; i < g_p; i++)
        printf("  fila %d: %d\n", i + 1, (int)g_consumidas[i]);

    printf("Tempo de operacao do mpmc: %.6f s\n",
           tempo_seg() - t_mpmc_inicio);

    ipc_destroy();
    printf("IPC removido. Encerrado.\n");
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "uso: %s p paradigma\n", argv[0]);
        return 1;
    }
    int p, paradigma;
    if (parse_int(argv[1], &p) || p < 1 || p > MAX_PRODUTORES) {
        fprintf(stderr, "erro - p deve estar entre 1 e %d\n", MAX_PRODUTORES);
        return 1;
    }
    if (parse_int(argv[2], &paradigma) ||
        (paradigma != PARADIGMA_SHM && paradigma != PARADIGMA_MSG)) {
        fprintf(stderr, "erro - paradigma deve ser 1 (shm) ou 2 (msg)\n");
        return 1;
    }

    /* Handler e recursos IPC precisam existir antes do primeiro fork. */
    g_mpmc_pid = getpid();
    instalar_handler();

    /* O SIGCHLD fica bloqueado e é lido pelo signalfd junto com o teclado.
     * Se o signalfd falhar (sfd = -1), o poll ignora o descritor e os filhos
     * são colhidos só a cada comando. */
    sigset_t so_chld;
    sigemptyset(&so_chld);
    sigaddset(&so_chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &so_chld, NULL);
    int sfd = signalfd(-1, &so_chld, SFD_NONBLOCK);

    /* Com buffer, linhas já lidas pelo stdio não apareceriam para o poll. */
    setvbuf(stdin, NULL, _IONBF, 0);

    if (ipc_init(paradigma, p) == -1) {
        perror("ipc_init");
        return 1;
    }

    double t_inicio = tempo_seg();

    char linha[256];
    while (1) {
        colher_filhos();

        printf(">> ");
        fflush(stdout);

        struct pollfd pf[2] = { { STDIN_FILENO, POLLIN, 0 }, { sfd, POLLIN, 0 } };
        while (poll(pf, 2, -1) != -1 || errno == EINTR) {
            if (pf[1].revents & POLLIN) {
                struct signalfd_siginfo si;
                while (read(sfd, &si, sizeof si) == (ssize_t)sizeof si)
                    ;
                colher_filhos();
            }
            if (pf[0].revents)          /* linha, EOF ou erro */
                break;
            pf[0].revents = pf[1].revents = 0;
        }

        if (fgets(linha, sizeof linha, stdin) == NULL) {
            printf("\n");               /* Ctrl-D encerra como o shutdown */
            break;
        }

        char *tok[8];
        int   ntok = 0;
        for (char *s = strtok(linha, " \t\r\n");
             s != NULL && ntok < 8;
             s = strtok(NULL, " \t\r\n"))
            tok[ntok++] = s;

        if (ntok == 0)
            continue;

        /* O '&' final é aceito e ignorado: os filhos já rodam em paralelo. */
        char  *ultimo = tok[ntok - 1];
        size_t tam    = strlen(ultimo);
        if (tam == 1 && ultimo[0] == '&')
            ntok--;
        else if (tam > 1 && ultimo[tam - 1] == '&')
            ultimo[tam - 1] = '\0';
        if (ntok == 0)
            continue;

        if (strcmp(tok[0], "shutdown") == 0)
            break;
        else if (strcmp(tok[0], "produz") == 0)
            cmd_produz_consome(TIPO_PRODUTOR, tok, ntok);
        else if (strcmp(tok[0], "consome") == 0)
            cmd_produz_consome(TIPO_CONSUMIDOR, tok, ntok);
        else
            printf(">>>> erro - comando desconhecido: %s\n", tok[0]);
    }

    cmd_shutdown(t_inicio);
    return 0;
}
