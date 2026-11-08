#!/bin/bash
# Os comandos vao com uma pausa entre eles, como se fossem digitados: mandados
# todos de uma vez, o shutdown mata os filhos antes de eles trabalharem.
echo "Teste de Borda: Buffer circular > 63 slots e ack=2 em fila vazia "
echo "Esperado: fila 1 com 100 consumidas; erro: consumidas 0 de 5"
{
    echo "produz 1 100 2";  sleep 0.5
    echo "consome 1 100 1"; sleep 1
    echo "consome 2 5 2";   sleep 1
    echo "shutdown"
} | ./mpmc 2 1
