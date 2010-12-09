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

rm -f *.out
rm -f train.* ref.* test.*
cp  data/train/input/* .


LIST="train.out train.twf train.pl1 train.pl2  train.pin "

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch  ${LIST}
  touch  train.tmp
  touch  train.cel
  touch  train.sav
  touch  train.sv2
fi

${PREFIX} $1 ${DASHDASH} train >stdout.out 2>stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp stdout.out  data/train/output/train.stdout
  for i in ${LIST}; do 
    cmp $i data/train/output/$i
  done
fi
echo "OK"

