#!/bin/bash

set -o nounset
set -o errexit

PREFIX=${PREFIX:-}
VERIFY=${VERIFY:-yes}
EMU_HACK=${EMU_HACK:-yes}



DASHDASH=""
if [[ "${PREFIX}" =~ sel_ldr ]] ; then
# TODO(robertm): remove -c option
# c.f.: http://code.google.com/p/nativeclient/issues/detail?id=717
 DASHDASH="-c --"
fi

rm -f  *.out *.s

if [[ "${EMU_HACK}" != "no" ]] ; then
  touch cp-decl.s
fi

${PREFIX} $1 ${DASHDASH} data/train/input/cp-decl.i -o cp-decl.s \
  > stdout.out 2> stderr.out

if [[ "${VERIFY}" != "no" ]] ; then
  echo "VERIFY"
  cmp  cp-decl.s  data/train/output/cp-decl.s
fi
echo "OK"
