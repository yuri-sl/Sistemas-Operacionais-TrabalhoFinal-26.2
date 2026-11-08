#!/bin/bash
# Os comandos vao com uma pausa entre eles, como se fossem digitados: mandados
# todos de uma vez, o shutdown mata os filhos antes de eles trabalharem.
echo "  Teste 3.2 (Fila de Mensagens - Validacao e Concorrencia); 2 produtores + fila inexistente "
echo "  Esperado: erro - fila 3 nao existe; fila 2 com 4 consumidas"
{
    echo "produz 3 2 1";  sleep 0.5
    echo "consome 2 2 1"; sleep 0.5
    echo "consome 2 2 1"; sleep 0.5
    echo "produz 2 5 2";  sleep 1
    echo "shutdown"
} | ./mpmc 2 2
