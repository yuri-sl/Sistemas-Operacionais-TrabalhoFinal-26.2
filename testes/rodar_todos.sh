#!/bin/bash
chmod +x testes/*.sh
make clean && make
make ipcclean

./testes/teste_3_1.sh
make ipcclean
sleep 1

./testes/teste_3_2.sh
make ipcclean
sleep 1

./testes/teste_3_3.sh
make ipcclean
sleep 1

./testes/teste_borda.sh
make ipcclean

echo "Todos os testes foram executados."