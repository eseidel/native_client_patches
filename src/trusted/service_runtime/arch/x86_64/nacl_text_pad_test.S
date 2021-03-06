/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

        .text
start_of_text:

        .p2align NACLENTRYALIGN
write_num_10:
        /* rdi has descriptor number, rsi has number to be printed */
        pushq %rbp
        movq %rsp, %rbp
        naclspadj $-0x20, %r15  /* buffer[0x20] */
        movl %ebp, %r8d         /* %r8 = &buffer[0x20] (off the end) */
        xorq %rcx, %rcx         /* %ecx = 0;  (byte count) */

        movq %rsi, %rax

        movq $10, %r9
do_digit_10:
        xorq %rdx, %rdx
        divq %r9                /* %rdx:%rax div_mod 10 */
        addq $'0', %rdx         /* *--%r8 = remainder + '0' */
        subl $1, %r8d
        movb %dl, (%r15, %r8)
        addq $1, %rcx           /* ++byte_count */
        andq %rax, %rax
        jnz do_digit_10

        movq %rcx, %rdx
        movq %r8, %rsi
        /* rdi still has descriptor number */
        call NACL_SYSCALL_ADDR(NACL_sys_write)

        movq %rbp, %rsp
        popq %r11
        naclrestbp %r11d, %r15
        popq %r11
        nacljmp %r11d, %r15

        .p2align NACLENTRYALIGN
write_num_16:
        pushq %rbp
        movq %rsp, %rbp
        naclspadj $-0x20, %r15
        movl %ebp, %r8d
        xorq %rcx, %rcx
do_digit_16:
        movq %rsi, %rax
        andq $0xf, %rax
        addq $'0', %rax
        cmpq $(10 + '0'), %rax
        jl do_digit_not_upper
        addq $('a'-'0'-10), %rax
do_digit_not_upper:
        subl $1, %r8d
        movb %al, (%r15, %r8)
        addq $1, %rcx
        shrq $4, %rsi
        jnz do_digit_16

        movq %rcx, %rdx
        movq %r8, %rsi
        /* rdi still has descriptor number */
        call NACL_SYSCALL_ADDR(NACL_sys_write)

        movq %rbp, %rsp
        popq %r11
        naclrestbp %r11d, %r15
        popq %r11
        nacljmp %r11d, %r15

        .p2align NACLENTRYALIGN
write_char:
        pushq %rbp
        movq %rsp, %rbp
        subl $0x10, %esp
        addq %r15, %rsp

        movl %esp, %r8d
        movb %sil, (%r15, %r8)
        movq $1, %rdx
        movl %esp, %esi
        /* %rdi retains value */
        call NACL_SYSCALL_ADDR(NACL_sys_write)

        movq %rbp, %rsp
        popq %r11
        naclrestbp %r11d, %r15
        popq %r11
        nacljmp %r11d, %r15

        .p2align NACLENTRYALIGN
        .globl _start
_start:
        movq $123456789012345, %rsi
        movq $1, %rdi
        call write_num_10

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char

        movq $0xdeadbeefcafebabe, %rsi
        movq $1, %rdi
        call write_num_16

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char

        leaq end_of_text, %rsi
        movq $1, %rdi
        call write_num_16

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char

        movq $0, %rdi
        call NACL_SYSCALL_ADDR(NACL_sys_sysbrk)

        xorq %r12, %r12  /* r12 holds our eventual exit status */
        cmpl $EXPECTED_BREAK, %eax
        je brk_sane
        movq $1, %r12
brk_sane:

        movq %rax, %rsi
        movq $1, %rdi
        call write_num_16

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char

        movq $EXPECTED_BREAK, %rsi
        movq $1, %rdi
        call write_num_16

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char

#if EXPECTED_RODATA != 0
        movq $12, %rdx
        movq $ro_str, %rsi
        movq $1, %rdi
        call NACL_SYSCALL_ADDR(NACL_sys_write)

        movq $ro_str, %rsi
        movq $1, %rdi
        call write_num_16

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char

        leaq ro_str, %rax
        cmpq $EXPECTED_RODATA, %rax
        je ro_sane
        movq $1, %r12
ro_sane:

        leaq ro_str, %rsi
        movq $1, %rdi
        call write_num_16

        movq $'\n', %rsi
        movq $1, %rdi
        call write_char
#endif

        movq %r12, %rdi
        call NACL_SYSCALL_ADDR(NACL_sys_exit)

        .skip TEXT_EXTEND - (. - start_of_text), 0xf4
end_of_text:

#if EXPECTED_RODATA != 0
        .section .rodata
ro_str: .ascii "Hello world\n"
        /*      123456789012 */
#endif
