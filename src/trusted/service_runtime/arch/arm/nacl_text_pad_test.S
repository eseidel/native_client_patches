/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

        /*
         * Code to test various address layout boundary conditions.
         * This is a translation from the x86-{32,64} code, and is
         * not intended to be super efficent, just good enough to
         * print out the values that are designed, while maintaining
         * strict control over the address space layout.
         */

        .cpu cortex-a8

        .text
start_of_text:
        /*
         * write_num_10 omitted.  __aeabi_udivmod is too long/complex
         * to replicate, we cannot link against glibc to get _udivsi3.o
         * and pick up transitive dependencies.  Plus, we don't actually
         * use decimal output (yet).
         *
         * One alternative would be to link using:
         *
         * ${LD} --native-client -e _start -Ttext 0x20000 -o ${TARGET} \
         *  ${SOURCES} ${GLIBC} ${TEXT_EXTEND}
         *
         * i.e., to load the text pad as a separate .o *after* external
         * references are resolved from glibc, with link-time
         * expansion/computation of the (TEXT_EXTEND - (. - start_of_text))/4
         * fill expression (is that possible?).
         */

        /*
         * write_num_16.  Write an unsigned value in hex.
         * r0 is descriptor number, r1 is unsigned value to write.
         */
        .p2align 4
write_num_16:
        push {r4, lr}
        mov r3, sp
        sub sp, sp, #0x10
        bic sp, sp, #0xc0000000

        mov r2, #0
do_digit_16:
        sub r3, r3, #1
        add r2, r2, #1
        and r4, r1, #0xf

        add r4, r4, #'0'
        cmp r4, #'9'
        ble do_digit_not_upper
        add r4, r4, #('a'-'0'-10)

do_digit_not_upper:
        bic r3, r3, #0xc0000000
        strb r4, [r3]
        lsrs r1, r1, #4
        bne do_digit_16

        mov r1, r3
        movw r3, #:lower16:NACL_SYSCALL_ADDR(NACL_sys_write)
        movt r3, #:upper16:NACL_SYSCALL_ADDR(NACL_sys_write)
        nop

        nop
        nop
        bic r3, r3, #0xc000000f
        blx r3

        add sp, sp, #0x10
        bic sp, sp, #0xc0000000
        pop {r4, lr}
        nop

        bic lr, lr, #0xc000000f
        bx lr

        .p2align 4
        /*
         * r0 is descriptor number, r1 is character to write
         */
write_char:
        push {lr}
        sub sp, sp, #0x10
        bic sp, sp, #0xc0000000
        strb r1, [sp]

        mov r1, sp
        mov r2, #1
        movw r3, #:lower16:NACL_SYSCALL_ADDR(NACL_sys_write)
        movt r3, #:upper16:NACL_SYSCALL_ADDR(NACL_sys_write)

        nop
        nop
        bic r3, r3, #0xc000000f
        blx r3

        add sp, sp, #0x10
        bic sp, sp, #0xc0000000
        pop {lr}
        nop

        bic lr, lr, #0xc000000f
        bx lr

        .p2align 4
        .globl _start
_start:
        /* basic I/O test */
        mov r0, #1
        movw r1, #0xbeef
        movt r1, #0xdead
        bl write_num_16

        mov r0, #1
        mov r1, #'\n'
        nop
        bl write_char

        movw r1, #:lower16:end_of_text
        movt r1, #:upper16:end_of_text
        mov r0, #1
        bl write_num_16

        mov r0, #1
        mov r1, #'\n'
        nop
        bl write_char

        mov r0, #0
        movw r3, #:lower16:NACL_SYSCALL_ADDR(NACL_sys_sysbrk)
        movt r3, #:upper16:NACL_SYSCALL_ADDR(NACL_sys_sysbrk)
        nop

        nop
        nop
        bic r3, r3, #0xc000000f
        blx r3

        mov r4, r0  /* save a copy of break addr */
        mov r1, r0
        mov r0, #1
        bl write_num_16

        mov r0, #1
        mov r1, #'\n'
        nop
        bl write_char

        mov r5, #0  /* r5 holds eventual exit status */
        movw r1, #:lower16:EXPECTED_BREAK
        movt r1, #:upper16:EXPECTED_BREAK
        cmp r1, r4

        movne r5, #1
        mov r0, #1  /* r1 still has EXPECTED_BREAK */
        nop
        bl write_num_16

        mov r1, #'\n'
        mov r0, #1
        nop
        bl write_char

#if EXPECTED_RODATA != 0
        mov r2, #12
        movw r1, #:lower16:ro_str
        movt r1, #:upper16:ro_str
        mov r0, #1

        movw r3, #:lower16:NACL_SYSCALL_ADDR(NACL_sys_write)
        movt r3, #:upper16:NACL_SYSCALL_ADDR(NACL_sys_write)
        bic r3, r3, #0xc000000f
        blx r3

        movw r1, #:lower16:ro_str
        movt r1, #:upper16:ro_str
        mov r0, #1
        bl write_num_16

        mov r1, #'\n'
        mov r0, #1
        nop
        bl write_char

        movw r1, #:lower16:EXPECTED_RODATA
        movt r1, #:upper16:EXPECTED_RODATA
        movw r2, #:lower16:ro_str
        movt r2, #:upper16:ro_str

        cmp r1, r2
        movne r5, #1
        mov r0, #1  /* r1 still has EXPECTED_RODATA */
        bl write_num_16

        mov r1, #'\n'
        mov r0, #1
        nop
        bl write_char
#endif

        movw r3, #:lower16:NACL_SYSCALL_ADDR(NACL_sys_exit)
        movt r3, #:upper16:NACL_SYSCALL_ADDR(NACL_sys_exit)
        mov r0, #0
        nop

        nop
        nop
        bic r3, r3, #0xc000000f
        blx r3

        /* use new proper halt */
        .fill (TEXT_EXTEND - (. - start_of_text))/4, 4, 0xe1266676
end_of_text:

#if EXPECTED_RODATA != 0
        .section .rodata
ro_str: .ascii "Hello world\n"
        /*      123456789012 */
#endif
