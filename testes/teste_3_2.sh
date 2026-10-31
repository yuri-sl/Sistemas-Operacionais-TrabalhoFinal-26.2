#!/bin/bash
echo "  Teste 3.2 (Fila de Mensagens - Validacao e Concorrencia); 2 produtores + fila inexistente "
./mpmc 2 2 << 'EOF'
produz 3 2 1
consome 2 2 1
consome 2 2 1
produz 2 5 2
shutdown
EOF