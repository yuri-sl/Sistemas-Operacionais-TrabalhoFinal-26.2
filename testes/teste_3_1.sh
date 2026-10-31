#!/bin/bash
echo " Teste 3.1 (Memoria Compartilhada) - Exemplo 3.1: SHM — 4 produtores "
./mpmc 4 1 << 'EOF'
produz 3 2 1
consome 2 2 1
produz 2 5 1
shutdown
EOF