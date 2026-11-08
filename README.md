# MPMC — Múltiplos Produtores / Múltiplos Consumidores

Trabalho prático de Sistemas Operacionais (UnB, 02/2026, Prof. Alba Melo).

O sistema tem p produtores e consumidores e usa um de dois paradigmas de IPC System V:

| Paradigma | Mecanismo | Estrutura por produtor |
|---|---|---|
| 1 | `shm` + `sem` | 1 segmento de 2048 bytes com buffer circular |
| 2 | `msg` | 1 fila de mensagens |

---

## Compilar e rodar

```bash
make            # gera ./mpmc
./mpmc 4 1      # 4 produtores, memória compartilhada
>> produz 2 5 1
>> consome 2 3 2
>> shutdown

make clean      # apaga binário e .o
make ipcclean   # apaga shm/sem/msg que sobraram de execuções que travaram
```

Rodem `ipcs` de vez em quando. Quando o programa trava, os recursos IPC continuam no sistema e o limite enche rápido. Isso inclui encerrar com Ctrl-C: só o `shutdown` (ou o fim da entrada, Ctrl-D) apaga o IPC.

Precisa de Linux (System V IPC e sinais de tempo real). No Windows, use o WSL; o gcc do MinGW não tem `sys/shm.h`.

### Lançar com `&`
O enunciado permite `mpmc p paradigma &`. Num terminal, porém, um processo em segundo plano não pode ler do teclado: o shell o suspende (`Stopped (tty input)`) e os comandos digitados vão para o bash. Para usar, traga o mpmc para o primeiro plano com `fg`:

```bash
./mpmc 2 2 &
fg
>> produz 2 3 2
```

O `&` no fim de `produz`/`consome` é aceito e ignorado: eles já rodam concorrentes ao mpmc.

### Comandos

| Comando | Significado |
|---|---|
| `mpmc p paradigma` | p produtores; paradigma 1 = shm, 2 = msg |
| `produz p n_msgs ack` | produtor p (1..p, ou 0 = striped) envia n_msgs (1..1024). ack=1 espera a confirmação de consumo; ack=2 não espera |
| `consome p n_msgs ack` | consome n_msgs da estrutura p. ack=1 bloqueia até completar; ack=2 retorna erro com quantas consumiu |
| `shutdown` | termina tudo, imprime os tempos e os totais e apaga o IPC |

---

## Arquitetura

```
              ┌──────────── mpmc (shell) ────────────┐
              │ ipc_init() antes de qualquer fork     │
              │ handler de SIG_CONSUMO → contadores   │
              └──────┬──────────────────────┬─────────┘
                fork │                      │ fork
              ┌──────▼──────┐        ┌──────▼──────┐
              │   produz    │        │   consome   │
              │ ipc_put()   │        │ ipc_get()   │
              └──────▲──────┘        └──┬───────┬──┘
                     │   SIG_ACK        │       │ SIG_CONSUMO
                     └──────────────────┘       └──────► mpmc
                          ipc.c (despacho)
                   ┌──────────┴──────────┐
               ipc_shm.c            ipc_msg.c
            (paradigma 1)         (paradigma 2)
```

| Arquivo | Responsável | Conteúdo |
|---|---|---|
| `common.h` | **todos** | Contrato: tipos, constantes, sinais e assinaturas |
| `ipc.c` | pronto | Despacho para o paradigma escolhido e validação da fila |
| `mpmc.c` | Alan | Shell, fork, tabela de filhos, shutdown |
| `ipc_shm.c` | Yuri | Backend de memória compartilhada + semáforos |
| `ipc_msg.c` | Felipe | Backend de fila de mensagens |
| `produz.c` | Gabriel | Processo produtor, modo striped, espera de ack |
| `consome.c` | João Santos | Processo consumidor, envio de ack, testes |

Os testes ficam em `testes/` (ver [Testes](#testes)).

---

## Contrato (`common.h`)

**Não alterem sem avisar o grupo antes.**

### Mensagem
```c
typedef struct {
    pid_t           pid;      // pid do produz que enviou
    int             seq;      // 1..n_msgs, por processo emissor
    int             ack;      // ACK_SIM → consumidor confirma ao produtor
    struct timespec t_envio;  // CLOCK_REALTIME no momento do put
} msg_t;                      // 32 bytes
```

### Interface de IPC
Produz e consome usam **apenas** estas funções e não sabem qual é o paradigma.

| Função | Retorno |
|---|---|
| `ipc_init(paradigma, p)` | 0 / -1 |
| `ipc_put(fila, &m)` | 0; bloqueia se estiver cheio |
| `ipc_get(fila, &m, bloqueante)` | 0; -1 com `errno=EAGAIN` se a fila estiver vazia e não bloqueante |
| `ipc_destroy()` | — |

Fila fora de `0..p-1` → -1 com `errno=EINVAL`.

### Convenções
- **Filas:** o usuário digita 1..p. Internamente tudo é **0..p-1**, e o shell converte. `produz 0` vira `FILA_STRIPED`.
- **Recursos IPC** são criados com `IPC_PRIVATE` pelo mpmc **antes do primeiro fork**. Os filhos herdam ids e `shmat`. Não usamos `ftok`.
- **Processos** são só `fork()`, sem `exec`. O filho chama `produz_main`/`consome_main`, que terminam com `_exit()`.
- **Tempo de cada filho:** medido pelo mpmc, do fork até o `waitpid`. Enquanto espera um comando, o mpmc também acorda com o `SIGCHLD` (lido por um `signalfd`) e colhe na hora o filho que terminou: o tempo não inclui a espera no prompt e nenhum filho fica zumbi. Filhos ainda vivos no `shutdown` contam até morrerem pelo `SIGTERM`.
- **Contagem de consumidas:** o consumidor envia `SIG_CONSUMO` ao mpmc com `sival_int = fila` a cada mensagem consumida.

### Por que sinais de tempo real
O ack usa `SIG_ACK = SIGRTMIN` via `sigqueue`. Sinais comuns (`SIGUSR1`) **não enfileiram**: se dois consumidores mandam ack ao mesmo tempo, o produtor recebe um só. Ele ficaria esperando o segundo para sempre. Sinais RT enfileiram, então chega um sinal por ack.

O produtor precisa chamar `ack_preparar()` (bloquear `SIG_ACK`) **antes do primeiro `ipc_put`**. A ação padrão de um sinal RT é matar o processo.

---

## Detalhes por paradigma

### 1 — Memória compartilhada
- Cabeçalho de 8 bytes (`head`, `tail`) + **63 slots** de 32 bytes = 2024 bytes, dentro dos 2048. Um `_Static_assert` garante isso.
- Um único `semget` com `3*p` semáforos, acessados pelas macros `SEM_MUTEX(i)`, `SEM_VAZIO(i)` e `SEM_CHEIO(i)`.
  Exemplo: p=4 → 12 semáforos; a fila interna 2 usa os índices 6, 7 e 8.
- put: `P(VAZIO) P(MUTEX) … V(MUTEX) V(CHEIO)`
- get: `P(CHEIO)` (com `IPC_NOWAIT` se não bloqueante) `P(MUTEX) … V(MUTEX) V(VAZIO)`

### 2 — Fila de mensagens
- `mq_msg_t { long mtype; msg_t m; }` com `mtype = 1`.
- `msgsnd(id, &x, sizeof(msg_t), 0)`: o tamanho **não** inclui o `mtype`.
- `msgrcv` com `IPC_NOWAIT` devolve `ENOMSG` quando a fila está vazia. Convertam para `EAGAIN`.
- O limite `msgmnb` (16384 bytes por padrão) comporta 512 mensagens de 32 bytes por fila. Acima disso o put bloqueia até alguém consumir, mesmo com `ack=2`; é o comportamento esperado.

---

## Armadilhas comuns
- **`EINTR`:** `semop`, `msgsnd` e `msgrcv` são interrompidos por sinais. Repitam a chamada.
- **Recursos órfãos:** sempre `make ipcclean` depois de um teste que travou.
- **`printf` em handler de sinal** não é seguro. No handler, só incrementem contadores.
- **Striped:** a mensagem `seq` vai para a fila `(seq-1) % p`.
  Exemplo: `produz 0 15 2` com p=5 → fila 0 recebe 1, 6, 11; fila 1 recebe 2, 7, 12; …

---

## Testes (Pessoa 5)

```bash
bash testes/rodar_todos.sh   # compila, roda os quatro testes e limpa o IPC
```

Para conferir cada fluxo à mão (semáforos, buffer cheio e vazio, ack, striped, zumbis, shutdown) com `ps` e `ipcs`, siga o [guia de verificação manual](GUIA_VERIFICACAO.md).

Reproduzir os exemplos do enunciado nos dois paradigmas:

| Teste | Entrada | Esperado |
|---|---|---|
| 3.1 | `mpmc 4 1` / `produz 3 2 1` / `consome 2 2 1` / `produz 2 5 1` / `shutdown` | consumidor completa e produtores ficam bloqueados até o shutdown |
| 3.2 | `mpmc 2 2` / `produz 3 2 1` | `erro - fila 3 nao existe` |
| 3.3 | `mpmc 5 2` / `produz 0 15 2` / `consome 2 3 1` / … | 3 mensagens por fila (striped) |

Casos de borda: `n_msgs` = 0 e 1025, `ack` inválido, consumo com `ack=2` em fila vazia, 1024 mensagens em memória compartilhada (precisa dar várias voltas no buffer de 63 slots).

---

## Dúvidas pendentes com a professora
1. Exemplo 3.1: "produtor 3 produz na memória compartilhada 2". É erro de digitação ou existe uma regra de mapeamento?
   Hoje: `produz 3` grava na fila 3. O resultado final do exemplo (2 consumidas na fila 2) é o mesmo nas duas leituras.
2. Último exemplo: o texto fala em "3 mensagens" e depois em "duas mensagens saírem do buffer".
   Hoje: `consome 2 3 1` consome 3, como o comando pede.
3. Mensagens striped contam como de qual produtor para efeito de ack?
   Hoje: o ack volta para o processo `produz` que enviou (pid na mensagem), qualquer que seja a fila.
