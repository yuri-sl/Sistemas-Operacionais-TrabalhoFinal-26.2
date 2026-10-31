#!/bin/bash
echo " Teste 3.3 Filas de Mensagem — Striped com 5 produtores"
./mpmc 5 2 << 'EOF'
produz 0 15 2
consome 2 3 1
consome 1 2 1
produz 3 2 1
shutdown
EOF