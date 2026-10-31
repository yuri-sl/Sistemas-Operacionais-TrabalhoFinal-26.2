/* mpmc.c — Pessoa 1: shell, gerência de processos e shutdown
 *
 * Papel do mpmc no trabalho (ver common.h e o enunciado):
 *   - É lançado como "mpmc p paradigma" (p = nº de produtores/filas,
 *     paradigma = 1 shm+sem, 2 fila de mensagens).
 *   - Cria TODOS os recursos IPC com ipc_init(), ANTES do primeiro fork,
 *     para que os filhos herdem os ids (e os mapeamentos de shm) por fork.
 *   - Instala o handler de SIG_CONSUMO ANTES do primeiro fork: cada mensagem
 *     consumida por um filho vira um sinal que incrementa g_consumidas[fila].
 *   - Mostra o prompt ">>" e interpreta: produz / consome / shutdown.
 *       produz  p n_msgs ack   (p = 0 -> striped; 1..g_p -> fila p-1)
 *       consome p n_msgs ack   (p = 1..g_p -> fila p-1)
 *       shutdown
 *   - Para cada produz/consome faz fork(); o filho chama produz_main()/
 *     consome_main() e termina com _exit(). O mpmc apenas registra o filho.
 *   - O tempo de execução de cada filho é medido PELO MPMC: início no fork,
 *     fim no waitpid (os filhos não têm handler de SIGTERM).
 *   - No shutdown: SIGTERM nos filhos vivos, waitpid em todos, imprime o tempo
 *     de cada filho, o total consumido por fila, o tempo de operação do mpmc,
 *     e por fim ipc_destroy() para apagar os IPCs.
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/wait.h>

/* -------------------------------------------- globais do contrato (common.h) */
int   g_paradigma;
int   g_p;
pid_t g_mpmc_pid;

/* -------------------------------------------------------- tabela de filhos */
#define TIPO_PRODUTOR    0
#define TIPO_CONSUMIDOR  1
#define MAX_FILHOS       4096          /* teto de processos criados na sessão */

typedef struct {
    pid_t  pid;
    int    tipo;        /* TIPO_PRODUTOR | TIPO_CONSUMIDOR                     */
    int    fila;        /* 0..g_p-1, ou FILA_STRIPED para "produz 0"          */
    int    n_msgs;
    double t_inicio;    /* tempo_seg() no fork                                */
    double t_fim;       /* tempo_seg() quando o waitpid o colheu             */
    int    vivo;        /* 1 enquanto ainda não foi colhido por waitpid       */
} filho_t;

static filho_t g_filhos[MAX_FILHOS];
static int     g_nfilhos;

/* Total de mensagens consumidas por fila; só o handler escreve, por isso
 * é volatile sig_atomic_t. Indexado pela fila interna (0..g_p-1). */
static volatile sig_atomic_t g_consumidas[MAX_PRODUTORES];

/* ------------------------------------------------------- handler SIG_CONSUMO
 * O consumidor faz sigqueue(mpmc, SIG_CONSUMO, {.sival_int = fila}) a cada
 * mensagem consumida. SIG_CONSUMO é um sinal de tempo real (enfileira), então
 * nenhuma contagem se perde mesmo com vários consumidores ao mesmo tempo. */
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
    sa.sa_flags     = SA_SIGINFO | SA_RESTART;  /* SA_SIGINFO: recebe sival_int */
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIG_CONSUMO, &sa, NULL) == -1) {
        perror("sigaction(SIG_CONSUMO)");
        exit(1);
    }
}

/* --------------------------------------------------------- utilitários */

/* Converte uma string inteira por completo; -1 se não for um inteiro válido. */
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

/* Colhe (sem bloquear) os filhos que já terminaram e anota o tempo de fim.
 * Chamado a cada volta do loop: mantém a tabela em dia e evita processos
 * zumbis enquanto o mpmc espera o próximo comando. */
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
    /* pid == -1 com errno ECHILD (nenhum filho) é esperado: ignoramos. */
}

/* fork() de um filho produtor/consumidor e registro na tabela. */
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
        /* Filho: executa a tarefa e termina. produz_main/consome_main já
         * chamam _exit(); o _exit abaixo é só uma rede de segurança. */
        if (tipo == TIPO_PRODUTOR) produz_main(fila, n_msgs, ack);
        else                       consome_main(fila, n_msgs, ack);
        _exit(0);
    }

    /* Pai: registra o filho recém-criado. */
    filho_t *f = &g_filhos[g_nfilhos++];
    f->pid      = pid;
    f->tipo     = tipo;
    f->fila     = fila;
    f->n_msgs   = n_msgs;
    f->t_inicio = t0;
    f->t_fim    = t0;
    f->vivo     = 1;
}

/* Valida e executa "produz p n_msgs ack" ou "consome p n_msgs ack".
 * tok[0] é o comando; espera exatamente 4 tokens (o '&' já foi removido). */
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

    /* ack: 1 (espera confirmacao) ou 2 (nao espera) */
    if (ack != ACK_SIM && ack != ACK_NAO) {
        printf(">>>> erro - ack deve ser 1 (sim) ou 2 (nao)\n");
        return;
    }
    /* n_msgs: 1..1024 */
    if (n_msgs < MIN_MSGS || n_msgs > MAX_MSGS) {
        printf(">>>> erro - n_msgs deve estar entre %d e %d\n",
               MIN_MSGS, MAX_MSGS);
        return;
    }

    /* Fila: o usuário digita 1..g_p; internamente é 0..g_p-1.
     * Só o produz aceita 0, que vira o padrão striped (FILA_STRIPED). */
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

/* Encerra o sistema: mata os filhos, espera todos, imprime o relatório e
 * apaga os recursos IPC. */
static void cmd_shutdown(double t_mpmc_inicio)
{
    /* 1. SIGTERM nos filhos que ainda estão vivos (ex.: produtor bloqueado
     *    esperando ack, ou consumidor bloqueado esperando mensagem). */
    for (int i = 0; i < g_nfilhos; i++)
        if (g_filhos[i].vivo)
            kill(g_filhos[i].pid, SIGTERM);

    /* 2. Espera (bloqueante) cada filho vivo terminar e anota o tempo de fim. */
    for (int i = 0; i < g_nfilhos; i++) {
        if (!g_filhos[i].vivo)
            continue;
        int status;
        while (waitpid(g_filhos[i].pid, &status, 0) == -1 && errno == EINTR)
            ;                                   /* um sinal interrompeu: repete */
        g_filhos[i].t_fim = tempo_seg();
        g_filhos[i].vivo  = 0;
    }

    /* 3. Tempo de execução de cada processo filho. */
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

    /* 4. Total de mensagens consumidas em cada fila (numeração do usuário). */
    printf("Mensagens consumidas por fila:\n");
    for (int i = 0; i < g_p; i++)
        printf("  fila %d: %d\n", i + 1, (int)g_consumidas[i]);

    /* 5. Tempo total de operação do mpmc. */
    printf("Tempo de operacao do mpmc: %.6f s\n",
           tempo_seg() - t_mpmc_inicio);

    /* 6. Apaga os recursos IPC (shm+sem ou filas de mensagem). */
    ipc_destroy();
    printf("IPC removido. Encerrado.\n");
}

int main(int argc, char **argv)
{
    /* -------- 1. valida "mpmc p paradigma" -------- */
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

    /* -------- 2. prepara estado ANTES do primeiro fork -------- */
    g_mpmc_pid = getpid();
    instalar_handler();                 /* handler de SIG_CONSUMO */

    if (ipc_init(paradigma, p) == -1) { /* cria os recursos IPC */
        perror("ipc_init");
        return 1;
    }

    double t_inicio = tempo_seg();

    /* -------- 3. loop do shell ">>" -------- */
    char linha[256];
    while (1) {
        colher_filhos();                /* anota quem terminou desde o último comando */

        printf(">> ");
        fflush(stdout);

        if (fgets(linha, sizeof linha, stdin) == NULL) {
            printf("\n");               /* EOF (Ctrl-D) -> trata como shutdown */
            break;
        }

        /* Tokeniza por espaços, tabs e nova-linha. */
        char *tok[8];
        int   ntok = 0;
        for (char *s = strtok(linha, " \t\r\n");
             s != NULL && ntok < 8;
             s = strtok(NULL, " \t\r\n"))
            tok[ntok++] = s;

        if (ntok == 0)
            continue;                   /* linha em branco */

        /* Remove o '&' final (lançamento em background não muda nada para o
         * pai: produz/consome já rodam concorrentes como processos à parte).
         * Aceita o '&' separado ("... 1 &") ou colado ("... 1&"). */
        char  *ultimo = tok[ntok - 1];
        size_t tam    = strlen(ultimo);
        if (tam == 1 && ultimo[0] == '&')
            ntok--;
        else if (tam > 1 && ultimo[tam - 1] == '&')
            ultimo[tam - 1] = '\0';
        if (ntok == 0)
            continue;

        /* -------- despacho dos comandos -------- */
        if (strcmp(tok[0], "shutdown") == 0)
            break;
        else if (strcmp(tok[0], "produz") == 0)
            cmd_produz_consome(TIPO_PRODUTOR, tok, ntok);
        else if (strcmp(tok[0], "consome") == 0)
            cmd_produz_consome(TIPO_CONSUMIDOR, tok, ntok);
        else
            printf(">>>> erro - comando desconhecido: %s\n", tok[0]);
    }

    /* -------- 4. encerramento -------- */
    cmd_shutdown(t_inicio);
    return 0;
}
