# MPMC — Múltiplos Produtores / Múltiplos Consumidores

Trabalho prático de Sistemas Operacionais.

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

Rode `ipcs` de vez em quando. Quando o programa trava, os recursos IPC continuam no sistema e o limite enche rápido. Isso inclui encerrar com Ctrl-C: só o `shutdown` (ou o fim da entrada, Ctrl-D) apaga o IPC.

```bash
./mpmc 2 2 &
fg
>> produz 2 3 2
```

### Comandos

| Comando | Significado |
|---|---|
| `mpmc p paradigma` | p produtores; paradigma 1 = shm, 2 = msg |
| `produz p n_msgs ack` | produtor p (1..p, ou 0 = striped) envia n_msgs (1..1024). ack=1 espera a confirmação de consumo; ack=2 não espera |
| `consome p n_msgs ack` | consome n_msgs da estrutura p. ack=1 bloqueia até completar; ack=2 retorna erro com quantas consumiu |
| `shutdown` | termina tudo, imprime os tempos e os totais e apaga o IPC |

---

## Arquitetura

| Arquivo  | Conteúdo |
|---|---|
| `common.h` | Contrato: tipos, constantes, sinais e assinaturas |
| `ipc.c` | Despacho para o paradigma escolhido e validação da fila |
| `mpmc.c`| Shell, fork, tabela de filhos, shutdown |
| `ipc_shm.c`| Backend de memória compartilhada + semáforos |
| `ipc_msg.c` | Backend de fila de mensagens |
| `produz.c` | Processo produtor, modo striped, espera de ack |
| `consome.c`| Processo consumidor, envio de ack, testes |

Os testes ficam em `testes/`.

---

### Mensagem
```c
typedef struct {
    pid_t           pid;      // pid do produz que enviou
    int             seq;      // 1..n_msgs, por processo emissor
    int             ack;      // ACK_SIM -> consumidor confirma ao produtor
    struct timespec t_envio;  // CLOCK_REALTIME no momento do put
} msg_t;                      // 32 bytes
```

### Interface de IPC
Produz e consome usam apenas estas funções e não sabem qual é o paradigma.

| Função | Retorno |
|---|---|
| `ipc_init(paradigma, p)` | 0 / -1 |
| `ipc_put(fila, &m)` | 0; bloqueia se estiver cheio |
| `ipc_get(fila, &m, bloqueante)` | 0; -1 com `errno=EAGAIN` se a fila estiver vazia e não bloqueante |
| `ipc_destroy()` | — |

Fila fora de `0..p-1` -> -1 com `errno=EINVAL`.

## Testes

```bash
bash testes/rodar_todos.sh   # compila, roda os quatro testes e limpa o IPC
```

Reproduzir os exemplos do enunciado nos dois paradigmas:

| Teste | Entrada | Esperado |
|---|---|---|
| 3.1 | `mpmc 4 1` / `produz 3 2 1` / `consome 2 2 1` / `produz 2 5 1` / `shutdown` | consumidor completa e produtores ficam bloqueados até o shutdown |
| 3.2 | `mpmc 2 2` / `produz 3 2 1` | `erro - fila 3 nao existe` |
| 3.3 | `mpmc 5 2` / `produz 0 15 2` / `consome 2 3 1` / … | 3 mensagens por fila (striped) |

Casos de borda: `n_msgs` = 0 e 1025, `ack` inválido, consumo com `ack=2` em fila vazia, 1024 mensagens em memória compartilhada (precisa dar várias voltas no buffer de 63 slots).

---
