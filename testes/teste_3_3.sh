#!/bin/bash
# Pausas entre os comandos: sem elas o shutdown chega antes de os filhos trabalharem.
echo "Teste 3.3 Filas de Mensagem - Striped com 5 produtores"
echo "Esperado: fila 1 com 2 consumidas, fila 2 com 3, demais com 0"
{
    echo "produz 0 15 2"; sleep 0.5
    echo "consome 2 3 1"; sleep 0.5
    echo "consome 1 2 1"; sleep 0.5
    echo "produz 3 2 1";  sleep 1
    echo "shutdown"
} | ./mpmc 5 2
