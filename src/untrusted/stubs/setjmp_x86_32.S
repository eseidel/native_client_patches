/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"


/*
 * jump_buf is organized as follows
 * ebx, ebp, esp, esi, edi, pc
 * NOTE: eax, ecx, edx are NOT preserved
 */
    .text
    .p2align NACLENTRYALIGN
    .globl   setjmp
    .type    setjmp, @function

/* int setjmp(jmp_buf env) */
setjmp:
    pop     %edx              /* return address */

    movl    0(%esp), %ecx     /* ecx has env argument*/

    movl    %ebx, 0(%ecx)     /* save ebx */
    movl    %ebp, 4(%ecx)     /* save ebp */
    movl    %esp, 8(%ecx)     /* save esp */
    movl    %esi, 12(%ecx)    /* save esi */
    movl    %edi, 16(%ecx)    /* save edi */
    movl    %edx, 20(%ecx)    /* save pc */

    xor     %eax, %eax        /* return zero */
    nacljmp %edx



    .text
    .p2align NACLENTRYALIGN
    .globl   longjmp
    .type    longjmp, @function

/* void longjmp(jmp_buf env, int val) */
longjmp:
    movl    4(%esp), %ecx     /* ecx has env argument */
    movl    8(%esp), %eax     /* eax has val argument */
    /* mov into %eax sets conditon codes */
    jne     .skip
    movl    $1, %eax
.skip:
    movl    0(%ecx), %ebx     /* restore ebx */
    movl    4(%ecx), %ebp     /* restore ebp */
    movl    8(%ecx), %esp     /* restore esp */
    movl    12(%ecx), %esi    /* restore esi */
    movl    16(%ecx), %edi    /* restore edi */
    movl    20(%ecx), %ecx
    nacljmp %ecx


