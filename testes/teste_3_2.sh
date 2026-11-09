#!/bin/bash
# Pausas entre os comandos: sem elas o shutdown chega antes de os filhos trabalharem.
echo "Teste 3.2 (Fila de Mensagens - Validacao e Concorrencia); 2 produtores + fila inexistente"
echo "Esperado: erro - fila 3 nao existe; fila 2 com 4 consumidas"
{
    echo "produz 3 2 1";  sleep 0.5
    echo "consome 2 2 1"; sleep 0.5
    echo "consome 2 2 1"; sleep 0.5
    echo "produz 2 5 2";  sleep 1
    echo "shutdown"
} | ./mpmc 2 2
