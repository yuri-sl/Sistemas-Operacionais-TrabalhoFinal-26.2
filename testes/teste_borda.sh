#!/bin/bash
# Pausas entre os comandos: sem elas o shutdown chega antes de os filhos trabalharem.
echo "Teste de Borda: Buffer circular > 63 slots e ack=2 em fila vazia"
echo "Esperado: fila 1 com 100 consumidas; erro: consumidas 0 de 5"
{
    echo "produz 1 100 2";  sleep 0.5
    echo "consome 1 100 1"; sleep 1
    echo "consome 2 5 2";   sleep 1
    echo "shutdown"
} | ./mpmc 2 1
