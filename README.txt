MPMC - Multiplos Produtores / Multiplos Consumidores
=====================================================

Integrantes:
Alan Henrique Rangel Ferreira
Felipe de Oliveira Fonseca Ribeiro
Joao Pedro Fernandes Santos
Gabriel Ribeiro da Silva
Yuri Santana Lopes

Trabalho pratico de Sistemas Operacionais.

O sistema tem p produtores e consumidores e usa um de dois paradigmas de
IPC System V:

  Paradigma 1 | shm + sem | 1 segmento de 2048 bytes com buffer circular
  Paradigma 2 | msg       | 1 fila de mensagens

-----------------------------------------------------------------------

COMPILAR E RODAR

Requer Linux (usa IPC System V: shm, sem, msg). Em Windows, use o WSL
(Ubuntu). Precisa de gcc e make instalados.

Compilar com uma unica linha de gcc (sem Makefile):

  gcc -std=c11 -Wall -Wextra -D_GNU_SOURCE -g -o mpmc \
      mpmc.c ipc.c ipc_shm.c ipc_msg.c produz.c consome.c

Isso gera o executavel ./mpmc. Os seis arquivos .c e o common.h precisam
estar na mesma pasta.

Se preferir, com o Makefile presente basta `make`.

Rodar:

  ./mpmc 4 1      # 4 produtores, memoria compartilhada
  >> produz 2 5 1
  >> consome 2 3 2
  >> shutdown

Limpeza:

  rm -f *.o mpmc          # apaga binario e objetos (equivale a `make clean`)

  # apaga shm/sem/msg que sobraram de execucoes que travaram
  # (equivale a `make ipcclean`):
  ipcs -m | awk '$3 == "'$USER'" {print $2}' | xargs -r -n1 ipcrm -m
  ipcs -s | awk '$3 == "'$USER'" {print $2}' | xargs -r -n1 ipcrm -s
  ipcs -q | awk '$3 == "'$USER'" {print $2}' | xargs -r -n1 ipcrm -q

Rode `ipcs` de vez em quando. Quando o programa trava, os recursos IPC
continuam no sistema e o limite enche rapido. Isso inclui encerrar com
Ctrl-C: so o `shutdown` (ou o fim da entrada, Ctrl-D) apaga o IPC.

  ./mpmc 2 2 &
  fg
  >> produz 2 3 2

Comandos:

  mpmc p paradigma
      p produtores; paradigma 1 = shm, 2 = msg

  produz p n_msgs ack
      produtor p (1..p, ou 0 = striped) envia n_msgs (1..1024).
      ack=1 espera a confirmacao de consumo; ack=2 nao espera

  consome p n_msgs ack
      consome n_msgs da estrutura p. ack=1 bloqueia ate completar;
      ack=2 retorna erro com quantas consumiu

  shutdown
      termina tudo, imprime os tempos e os totais e apaga o IPC

-----------------------------------------------------------------------

ARQUITETURA

  common.h    Contrato: tipos, constantes, sinais e assinaturas
  ipc.c       Despacho para o paradigma escolhido e validacao da fila
  mpmc.c      Shell, fork, tabela de filhos, shutdown
  ipc_shm.c   Backend de memoria compartilhada + semaforos
  ipc_msg.c   Backend de fila de mensagens
  produz.c    Processo produtor, modo striped, espera de ack
  consome.c   Processo consumidor, envio de ack, testes

Os testes ficam em testes/.

-----------------------------------------------------------------------

MENSAGEM

  typedef struct {
      pid_t           pid;      // pid do produz que enviou
      int             seq;      // 1..n_msgs, por processo emissor
      int             ack;      // ACK_SIM -> consumidor confirma ao produtor
      struct timespec t_envio;  // CLOCK_REALTIME no momento do put
  } msg_t;                      // 32 bytes

INTERFACE DE IPC

Produz e consome usam apenas estas funcoes e nao sabem qual e o paradigma.

  ipc_init(paradigma, p)          0 / -1
  ipc_put(fila, &m)               0; bloqueia se estiver cheio
  ipc_get(fila, &m, bloqueante)   0; -1 com errno=EAGAIN se a fila estiver
                                  vazia e nao bloqueante
  ipc_destroy()                   -

Fila fora de 0..p-1 -> -1 com errno=EINVAL.

-----------------------------------------------------------------------

TESTES

  bash testes/rodar_todos.sh   # compila, roda os quatro testes e limpa o IPC

Reproduzir os exemplos do enunciado nos dois paradigmas:

  Teste 3.1
    Entrada:  mpmc 4 1 / produz 3 2 1 / consome 2 2 1 / produz 2 5 1 / shutdown
    Esperado: consumidor completa e produtores ficam bloqueados ate o shutdown

  Teste 3.2
    Entrada:  mpmc 2 2 / produz 3 2 1
    Esperado: erro - fila 3 nao existe

  Teste 3.3
    Entrada:  mpmc 5 2 / produz 0 15 2 / consome 2 3 1 / ...
    Esperado: 3 mensagens por fila (striped)

Casos de borda: n_msgs = 0 e 1025, ack invalido, consumo com ack=2 em fila
vazia, 1024 mensagens em memoria compartilhada (precisa dar varias voltas no
buffer de 63 slots).

-----------------------------------------------------------------------

APENDICE: COMO RECRIAR O MAKEFILE (opcional)

O Makefile nao foi incluido no envio (so .c/.h/.txt), mas e opcional: a linha
de gcc da secao COMPILAR E RODAR ja basta. Se quiser usar `make`, crie um
arquivo chamado exatamente "Makefile" (sem extensao) na mesma pasta dos
fontes e cole o conteudo abaixo.

ATENCAO: as linhas de receita (as que comecam logo abaixo de um alvo, como
a linha do gcc sob "mpmc:") PRECISAM comecar com um caractere TAB, nao com
espacos. Se usar espacos, o make falha com "missing separator". Ao colar num
editor, confirme que ha um TAB real no inicio dessas linhas.

------------------------ inicio do Makefile ------------------------
CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -D_GNU_SOURCE -g
OBJS   = mpmc.o ipc.o ipc_shm.o ipc_msg.o produz.o consome.o

mpmc: $(OBJS)
<TAB>$(CC) $(CFLAGS) -o $@ $^

%.o: %.c common.h
<TAB>$(CC) $(CFLAGS) -c $<

clean:
<TAB>rm -f *.o mpmc

# Limpa recursos IPC que sobrarem de execucoes que travaram
ipcclean:
<TAB>ipcs -m | awk '$$3 == "'$$USER'" {print $$2}' | xargs -r -n1 ipcrm -m
<TAB>ipcs -s | awk '$$3 == "'$$USER'" {print $$2}' | xargs -r -n1 ipcrm -s
<TAB>ipcs -q | awk '$$3 == "'$$USER'" {print $$2}' | xargs -r -n1 ipcrm -q

.PHONY: clean ipcclean
------------------------- fim do Makefile --------------------------

Onde aparece <TAB> acima, substitua por um caractere de tabulacao real.

Alternativa pronta (gera o Makefile sem precisar acertar os TABs na mao):
cole o bloco abaixo EXATAMENTE como esta (alinhado a esquerda, sem adicionar
espacos) no terminal. As receitas estao com 4 espacos e o `sed` final converte
esses 4 espacos iniciais em TAB. O terminador EOF precisa ficar na coluna 0.

cat > Makefile <<'EOF'
CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -D_GNU_SOURCE -g
OBJS   = mpmc.o ipc.o ipc_shm.o ipc_msg.o produz.o consome.o

mpmc: $(OBJS)
    $(CC) $(CFLAGS) -o $@ $^

%.o: %.c common.h
    $(CC) $(CFLAGS) -c $<

clean:
    rm -f *.o mpmc

ipcclean:
    ipcs -m | awk '$3 == "'$USER'" {print $2}' | xargs -r -n1 ipcrm -m
    ipcs -s | awk '$3 == "'$USER'" {print $2}' | xargs -r -n1 ipcrm -s
    ipcs -q | awk '$3 == "'$USER'" {print $2}' | xargs -r -n1 ipcrm -q

.PHONY: clean ipcclean
EOF
sed -i 's/^    /\t/' Makefile

Se isso parecer complicado, use a linha de gcc direta; ela compila sem
precisar de Makefile.
