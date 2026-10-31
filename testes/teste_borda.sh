#!/bin/bash
echo "Teste de Borda: Buffer circular > 63 slots e ack=2 em fila vazia "
./mpmc 2 1 << 'EOF'
produz 1 100 2
consome 1 100 1
consome 2 5 2
shutdown
EOF