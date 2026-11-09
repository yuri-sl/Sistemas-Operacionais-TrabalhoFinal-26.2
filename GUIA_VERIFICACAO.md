# Guia de verificação manual do mpmc

Roteiro para conferir, passo a passo, se cada fluxo do `mpmc` funciona. Para cada fluxo há uma explicação do que acontece por dentro, os comandos e o que você deve ver. Os resultados esperados foram conferidos no WSL (Ubuntu, gcc 15).

Para uma checagem rápida, sem inspecionar nada: `bash testes/rodar_todos.sh`.

---

## Preparação


Abra **dois terminais** e, nos dois:

```bash
wsl -d Ubuntu
cd /mnt/c/caminho/para/Sistemas-Operacionais-TrabalhoFinal-26.2
```

Confira com `uname`: tem que responder `Linux`.

- **Terminal A:** roda o `mpmc`.
- **Terminal B:** observa o sistema.

**Terminal A**, para compilar e limpar sobras:

```bash
make clean && make && make ipcclean
```

O programa é `./mpmc`, sem `.c`. `./mpmc.c` tenta executar o código-fonte como script de shell e dá erro.

**Terminal B**, para criar os atalhos de inspeção (valem até fechar o terminal):

```bash
filhos() { ps -o pid,ppid,stat,wchan:16,args -p "$(pgrep -xo mpmc)" --ppid "$(pgrep -xo mpmc)"; }
sems() { ipcs -s -i "$(ipcs -s | awk '/^0x/{print $2; exit}')" | awk '$1 ~ /^[0-9]+$/'; }
gcc -D_GNU_SOURCE -I. -o /tmp/ver_shm testes/ver_shm.c
```

- **`filhos`** lista o `mpmc` e os processos que ele criou.
  - `STAT`: `S` = dormindo, `Z` = zumbi.
  - `WCHAN`: em que ponto do kernel o processo está esperando.
- **`sems`** mostra uma linha por semáforo, com 5 colunas: número, valor, quantos processos estão esperando (`ncount`), `zcount` e o último pid.
- **`/tmp/ver_shm <shmid>`** mostra o conteúdo de um segmento de memória compartilhada. Ele só lê: anexa o segmento com `SHM_RDONLY` e não altera nada.

Os atalhos supõem **um `mpmc` por vez**. Antes de começar, confira no B que não sobrou nada:

```bash
ipcs -m -s -q; pgrep mpmc
```

O `ipcs` deve vir sem linhas `0x...` e o `pgrep` não deve imprimir nada.

### Como ler os semáforos

Com `p` filas, a fila *k* (como o usuário digita, 1..p) usa 3 semáforos:

| Semáforo | Nome | Valor inicial | Significado |
|---|---|---|---|
| 3(k−1) | MUTEX | 1 | 1 = ninguém está mexendo no buffer agora |
| 3(k−1)+1 | VAZIO | 63 | posições livres |
| 3(k−1)+2 | CHEIO | 0 | mensagens esperando consumo |

Com `p=2`, a fila 1 usa os semáforos 0, 1 e 2, e a fila 2 usa 3, 4 e 5. **VAZIO + CHEIO sempre somam 63.**

---

## Parte 1: memória compartilhada (paradigma 1)

Os fluxos 1 a 10 são feitos **na mesma sessão** do `mpmc`, em sequência. Os totais do fluxo 10 dependem disso.

### Fluxo 1. Iniciar o mpmc

**O que acontece:** o `mpmc` valida os argumentos e cria os recursos IPC **antes do primeiro `fork`**, para que os filhos os herdem. No paradigma 1, são `p` segmentos de 2048 bytes e 1 conjunto com `3p` semáforos. Depois ele mostra o prompt `>>`.

**Argumentos inválidos (A):**

```bash
./mpmc
./mpmc 0 1
./mpmc 2 3
```

| Comando | Esperado |
|---|---|
| `./mpmc` | `uso: ./mpmc p paradigma` |
| `./mpmc 0 1` | `erro - p deve estar entre 1 e 64` |
| `./mpmc 2 3` | `erro - paradigma deve ser 1 (shm) ou 2 (msg)` |

Nenhum deles cria recurso IPC.

**Início válido (A):**

```bash
./mpmc 2 1
```

**Conferir (B):**

```bash
ipcs -m -s
sems
filhos
```

Esperado:
- **Memória:** 2 segmentos com `bytes 2048`.
- **Semáforos:** 1 conjunto com `nsems 6`, valores `1 63 0 1 63 0`.
- **Processos:** só o `mpmc`, com `WCHAN do_sys_poll`, esperando você digitar.

### Fluxo 2. Validação dos comandos

**O que acontece:** cada comando é conferido antes de criar processo. Se houver erro, aparece a mensagem e nenhum filho é criado.

| Digite no A | Esperado |
|---|---|
| `produz 1 0 1` | `erro - n_msgs deve estar entre 1 e 1024` |
| `produz 1 1025 1` | `erro - n_msgs deve estar entre 1 e 1024` |
| `produz 1 5 3` | `erro - ack deve ser 1 (sim) ou 2 (nao)` |
| `produz 3 5 1` | `erro - fila 3 nao existe` |
| `consome 0 1 1` | `erro - fila 0 nao existe` (só o `produz` aceita 0) |
| `produz 1 x 1` | `erro - argumentos devem ser numeros inteiros` |
| `produz 1 2` | `erro - uso: produz p n_msgs ack` |
| `abc` | `erro - comando desconhecido: abc` |

**Conferir (B):** `filhos` continua mostrando só o `mpmc`.

### Fluxo 3. Produzir

**O que acontece:**
1. O `mpmc` faz `fork()` e volta ao prompt na hora.
2. O filho monta `n` mensagens, cada uma com `pid` (dele), `seq` (1..n), `ack` e o horário do envio.
3. Cada mensagem entra na memória compartilhada assim: `P(VAZIO)` (pega uma posição livre), `P(MUTEX)` (entra), grava, `V(MUTEX)` (sai), `V(CHEIO)` (avisa que há uma mensagem a mais).
4. Com `ack=2`, o filho termina logo depois de enviar.

**Faça (A):**

```
produz 1 5 2
```

**Conferir (B):**

```bash
sems
for id in $(ipcs -m | awk '/^0x/{print $2}'); do /tmp/ver_shm $id; echo; done
```

O `ver_shm` roda em todos os segmentos porque a ordem em que o `ipcs -m` os lista não é a ordem das filas: o primeiro da lista pode ser a fila 2.

Esperado:
- **Semáforos:** sem 1 (VAZIO da fila 1) = **58** e sem 2 (CHEIO) = **5**.
- **`ver_shm`:** os dois segmentos mostram `sizeof(msg_t) = 32 bytes` e `segmento ...: 2048 bytes`.
  - **Fila 1:** `head=0 tail=5` e 5 slots com o mesmo `pid`, `seq` de 1 a 5, `ack=2` e horários crescentes.
  - **Fila 2:** `head=0 tail=0` e nenhum slot.

### Fluxo 4. Consumir

**O que acontece:** o filho tira `n` mensagens da fila. No consumidor, o `ack` define o modo de espera:

- **`ack=1` (bloqueante):** com a fila vazia, ele espera chegar mensagem.
- **`ack=2` (não bloqueante):** com a fila vazia, ele desiste e imprime `erro: consumidas X de n`.

Para cada mensagem consumida, o filho:
1. Manda `SIG_ACK` ao produtor, usando o `pid` gravado na mensagem, se a **mensagem** pede confirmação (`ack=1` no `produz`).
2. Manda `SIG_CONSUMO` ao `mpmc`, que soma 1 no total daquela fila. É o total que aparece no `shutdown`.

**Faça (A):**

```
consome 1 5 1
```

**Conferir (B)**, depois de uns 2 segundos:

```bash
sems
filhos
```

Esperado:
- **Semáforos:** de volta a `1 63 0`.
- **Processos:** só o `mpmc`. Os filhos terminaram e foram colhidos, sem deixar zumbi.

**Não bloqueante com a fila vazia (A):**

```
consome 1 5 2
```

Esperado: `erro: consumidas 0 de 5`, na hora.

### Fluxo 5. Confirmação (ack)

**O que acontece:** um `produz` com `ack=1` envia tudo e depois **espera uma confirmação por mensagem**. Ele só termina quando todas chegam.

**Faça (A):**

```
produz 2 3 1
```

**Conferir (B):** `filhos` mostra o produtor com `S` e **`do_sigtimedwait`**, esperando os sinais.

**Faça (A):**

```
consome 2 3 1
```

**Conferir (B):** `filhos` mostra só o `mpmc`. O consumidor confirmou as 3 mensagens e os dois terminaram.

A confirmação usa um sinal de tempo real (`SIGRTMIN`), que **enfileira**. Se dois consumidores confirmarem ao mesmo tempo, os dois sinais chegam. Com `SIGUSR1`, um se perderia e o produtor esperaria para sempre.

### Fluxo 6. Buffer cheio

**O que acontece:** o buffer tem 63 posições. Quando enchem, o `P(VAZIO)` bloqueia e o produtor **dorme** até alguém consumir, sem perder mensagem e sem dar erro.

**Faça (A):**

```
produz 1 100 2
```

**Conferir (B):**

```bash
sems
filhos
```

Esperado:
- **Semáforos:**
  - sem 1 (VAZIO) = **0** com **ncount 1** (um processo esperando posição livre);
  - sem 2 (CHEIO) = **63**.
- **Processos:** o produtor dorme em **`do_semtimedop`**. Entraram 63 mensagens e faltam 37.

**Faça (A):**

```
consome 1 100 1
```

**Conferir (B):** o consumidor abre espaço, o produtor envia as 37 restantes e os dois terminam. `sems` volta a `1 63 0`.

### Fluxo 7. Buffer vazio

**O que acontece:** um consumidor bloqueante com a fila vazia dorme no `P(CHEIO)` até alguém produzir.

**Faça (A):**

```
consome 2 2 1
```

**Conferir (B):**
- **Semáforos:** `sems` mostra sem 5 (CHEIO da fila 2) = **0** com **ncount 1**.
- **Processos:** em `filhos`, o consumidor está em `do_semtimedop`.

**Faça (A):**

```
produz 2 2 2
```

**Conferir (B):** os dois somem de `filhos`.

### Fluxo 8. Striped

**O que acontece:** `produz 0` distribui as mensagens em rodízio entre as filas. A mensagem `seq` vai para a fila `((seq−1) mod p) + 1`. Com p=2, as mensagens 1 e 3 vão para a fila 1, e as 2 e 4 para a fila 2.

**Faça (A):**

```
produz 0 4 2
```

**Conferir (B):** `sems` mostra os semáforos 2 e 5 (CHEIO das duas filas) = **2** cada.

**Faça (A):**

```
consome 1 2 1
consome 2 2 1
```

### Fluxo 9. Zumbis e tempo de cada filho

**O que acontece:** quando um filho termina, o kernel avisa o `mpmc` com o sinal `SIGCHLD`. O `mpmc` espera o teclado e esse aviso ao mesmo tempo (`poll` + `signalfd`). Ao receber o aviso, acorda, chama `waitpid` e anota a hora do fim. Por isso:

- **Nenhum zumbi** fica na tabela de processos, mesmo com o `mpmc` parado no prompt.
- **O tempo do filho** vai do `fork` até ele terminar, sem contar a espera no prompt.

**Faça (A)**, um comando de cada vez, e depois **espere uns 5 segundos sem digitar**:

```
produz 1 3 2
consome 1 3 1
```

**Conferir (B):**

```bash
filhos
ps -eo stat | grep -c '^Z'
```

Esperado:
- **Processos:** só o `mpmc`.
- **Zumbis:** `0`.

No relatório do fluxo 10, esses dois filhos aparecem com tempo menor que 1 ms, mesmo com os 5 segundos parados.

### Fluxo 10. Shutdown

**O que acontece:**
1. `SIGTERM` nos filhos ainda vivos, por exemplo um produtor esperando ack ou um consumidor esperando mensagem.
2. `waitpid` em todos.
3. O relatório: quantos processos foram criados, o tempo de cada um, o total consumido por fila e o tempo total do `mpmc`.
4. A remoção de todos os recursos IPC.

**Faça (A):** deixe um produtor esperando um ack que não virá e encerre.

```
produz 1 1 1
shutdown
```

Esperado, se você fez os fluxos 1 a 9 nesta sessão:

```
Processos criados: 15
  [..] produz  ... tempo=0.000xxx s      <- quem terminou logo
  [..] produz  ... tempo=0.5xxxxx s      <- quem esperou (ack, buffer cheio)
Mensagens consumidas por fila:
  fila 1: 110        <- 5 (fluxo 4) + 100 (fluxo 6) + 2 (fluxo 8) + 3 (fluxo 9)
  fila 2: 7          <- 3 (fluxo 5) + 2 (fluxo 7) + 2 (fluxo 8)
Tempo de operacao do mpmc: ...
IPC removido. Encerrado.
```

**Conferir (B):**

```bash
ipcs -m -s -q; pgrep mpmc; ps -eo stat | grep -c '^Z'
```

Esperado:
- **IPC:** o `ipcs` vem sem linhas `0x...`.
- **`mpmc`:** o `pgrep` não imprime nada.
- **Zumbis:** `0`.

---

## Parte 2: fila de mensagens (paradigma 2)

Os comandos e o comportamento são os mesmos. Muda o mecanismo: cada fila é uma fila de mensagens do kernel, observada com `ipcs -q`.

**Faça (A):**

```bash
./mpmc 2 2
```

Depois rode a tabela abaixo em ordem. Use `ipcs -q` e `filhos` no B após cada linha.

| Comando (A) | `ipcs -q` (B) | `WCHAN` do filho |
|---|---|---|
| (logo após iniciar) | 2 filas, `0` bytes, `0` mensagens | — |
| `produz 1 5 2` | fila 1: `160` bytes, `5` mensagens | — |
| `produz 2 1024 2` | fila 2: `16384` bytes, `512` mensagens (cheia) | `do_msgsnd` |
| `consome 2 1024 1` | fila 2 volta a `0` | — |
| `consome 1 5 1` | fila 1 volta a `0` | — |
| `consome 2 2 1` | — | `do_msgrcv` (fila vazia) |
| `produz 2 2 2` | — | o consumidor termina |
| `consome 1 3 2` | — | imprime `erro: consumidas 0 de 3` |

A fila de mensagens enche com **512 mensagens**, não 63. É o limite do kernel: `msgmnb` = 16384 bytes, dividido por 32 bytes por mensagem.

**Encerre com Ctrl-D**, o fim da entrada. O relatório deve ser o mesmo do `shutdown`, com `fila 1: 5`, `fila 2: 1026` e `IPC removido`.

---

## Parte 3: Ctrl-C deixa sobras

O Ctrl-C mata o `mpmc` sem passar pelo `shutdown`, então **os recursos IPC ficam no sistema**.

**Faça (A):**

```bash
./mpmc 1 1
```

Aperte **Ctrl-C**. No B, `ipcs -m -s` ainda mostra o segmento e os semáforos. Limpe com:

```bash
make ipcclean
```

Encerre sempre com `shutdown` ou Ctrl-D.

---

## Resumo: onde cada coisa aparece

| Quero ver… | Comando (B) | Normal | Problema |
|---|---|---|---|
| Recursos criados | `ipcs -m -s -q` | `p` recursos enquanto roda; nada depois do `shutdown` | sobra depois de encerrar |
| Estado do buffer (shm) | `sems` | VAZIO + CHEIO = 63 | soma diferente de 63 |
| Quem está esperando o quê | `filhos` (coluna `WCHAN`) | `do_semtimedop`, `do_msgsnd`, `do_msgrcv` ou `do_sigtimedwait` só enquanto há motivo | processo parado sem motivo |
| Zumbis | `ps -eo stat \| grep -c '^Z'` | `0` | linhas `Z` / `<defunct>` |
| Conteúdo das mensagens | `/tmp/ver_shm <shmid>` (um por segmento; a ordem do `ipcs -m` não é a das filas) | pid, seq, ack e horário preenchidos | campos zerados ou trocados |
