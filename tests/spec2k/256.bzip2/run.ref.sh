#!/bin/bash

set -o nounset
set -o errexit

VERIFY=${PREFIX:-yes}
PREFIX=${PREFIX:-}

DASHDASH=""
if [[ "${PREFIX}" =~ sel_ldr ]] ; then
  DASHDASH="--"
fi

rm -f  *.out

${PREFIX} $1  ${DASHDASH} data/ref/input/input.source 58 > input.source.out  2>stderr1.out
${PREFIX} $1  ${DASHDASH} data/ref/input/input.graphic 58 > input.graphic.out  2>stderr2.out
${PREFIX} $1  ${DASHDASH} data/ref/input/input.program 58 > input.program.out  2>stder3.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  input.source.out  data/ref/output/input.source.out
  cmp  input.graphic.out  data/ref/output/input.graphic.out
  cmp  input.program.out  data/ref/output/input.program.out
fi
echo OK
