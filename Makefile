CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -D_GNU_SOURCE -g
OBJS   = mpmc.o ipc.o ipc_shm.o ipc_msg.o produz.o consome.o

mpmc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c common.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o mpmc

# Limpa recursos IPC que sobrarem de execuções que travaram
ipcclean:
	ipcs -m | awk '$$3 == "'$$USER'" {print $$2}' | xargs -r -n1 ipcrm -m
	ipcs -s | awk '$$3 == "'$$USER'" {print $$2}' | xargs -r -n1 ipcrm -s
	ipcs -q | awk '$$3 == "'$$USER'" {print $$2}' | xargs -r -n1 ipcrm -q

.PHONY: clean ipcclean
