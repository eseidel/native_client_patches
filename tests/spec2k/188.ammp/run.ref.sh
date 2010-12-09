#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}


DASHDASH=""
if [[ "${PREFIX}" =~ sel_ldr ]] ; then
  DASHDASH="--"
fi

rm -f *.out all.init.ammp ammp.in ammp.out
rm -f init_cond.run.1 init_cond.run.2 init_cond.run.3

ln -s  data/ref/input/* .

${PREFIX} $1 ${DASHDASH} <ammp.in  >ammp.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  ../specdiff.sh -r 0.003 -a 0.0001 -l 10 ammp.out data/ref/output/ammp.out
fi

echo "OK"
