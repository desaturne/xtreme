#!/bin/bash
# par2.sh "A" "B" jobs seedoffset : 10 games per job
A="$1"; B="$2"; j=$3; off=${4:-0}
for s in $(seq 1 $j); do python3 judge.py "$A" "$B" 10 $((s*101+off)) 2>/dev/null | tail -1 & done | awk '{s+=$2; m=($4>m?$4:m); c++} END {printf "AVG %.3f maxt %.2f games %d\n", s/c, m, c*10}'
