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

rm -f *.out *.err a10.img c756hel.in hc.img

ln -s  data/ref/input/* .

# First run
${PREFIX} $1 ${DASHDASH} -scanfile c756hel.in\
                         -trainfile1 a10.img\
                         -trainfile2 hc.img\
                         -stride 2\
                         -startx 110\
                         -starty 200\
                         -endx 160\
                         -endy 240\
                         -objects 10 > ref.1.out 2> ref.1.err

# Second run
${PREFIX} $1 ${DASHDASH} -scanfile c756hel.in\
                         -trainfile1 a10.img\
                         -trainfile2 hc.img\
                         -stride 2\
                         -startx 470\
                         -starty 140\
                         -endx 520\
                         -endy 180\
                         -objects 10 > ref.2.out 2> ref.2.err


if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  ../specdiff.sh -r 0.01 -l 10 ref.1.out data/ref/output/ref.1.out
  ../specdiff.sh -r 0.01 -l 10 ref.2.out data/ref/output/ref.2.out
fi

echo "OK"
