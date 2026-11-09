#!/bin/bash
# Pausas entre os comandos: sem elas o shutdown chega antes de os filhos trabalharem.
echo "Teste 3.1 (Memoria Compartilhada) - Exemplo 3.1: SHM - 4 produtores"
echo "Esperado: fila 2 com 2 consumidas, demais com 0"
{
    echo "produz 3 2 1";  sleep 0.5
    echo "consome 2 2 1"; sleep 0.5
    echo "produz 2 5 1";  sleep 1
    echo "shutdown"
} | ./mpmc 4 1
