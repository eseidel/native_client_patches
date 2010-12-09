/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
  Test integration of Valgrind and NaCl.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* For NaCl we have to use a special verion of valgrind.h */
#include "native_client/src/third_party/valgrind/nacl_valgrind.h"
#include "native_client/src/third_party/valgrind/nacl_memcheck.h"

#define NOINLINE __attribute__((noinline))
#define INLINE __attribute__((always_inline))

#define SHOW_ME do { \
    fprintf(stderr, "========== %s:%d =========\n",\
                             __FUNCTION__, __LINE__);  \
    fflush(stderr); \
  } while(0)

NOINLINE void break_optimization() {
  static volatile int z;
  z++;
}

/*----------------------------------------------------------------------
 Tests to see if memcheck finds memory bugs.
*/

/* Use the memory location '*a' in a conditional statement.
   Use macro instead of a function so that no extra stack frame is created. */
#define USE(ptr) do { \
  fprintf(stderr, "USE: %p\n", (void*)(ptr)); \
  if (*(ptr) == 777) { /* Use *a in a conditional statement. */ \
    fprintf(stderr, "777\n");                                   \
  }                                                             \
} while(0)

/* write something to '*a' */
void update(int *a) {
  *a = 1;
}

/* read from heap memory out of bounds */
void oob_read_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  USE(foo+10);
  USE(foo-1);
  free(foo);
}

/* write to heap memory out of bounds */
void oob_write_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  update(foo+10);
  free(foo);
}

/* read uninitialized value from heap */
void umr_heap_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  USE(foo+5);
  free(foo);
}

/* read uninitialized value from stack */
void umr_stack_test() {
  int foo[10];
  SHOW_ME;
  USE(foo+5);
}

/* use heap memory after free() */
void use_after_free_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  foo[5] = 666;
  free(foo);
  USE(foo+5);
}

/* use heap memory after free() and a series of unrelated malloc/free */
void use_after_free_with_reuse_test() {
  int i;
  int *foo;
  int *bar[1000];
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  foo[5] = 666;
  free(foo);
  for (i = 0; i < 1000; i++) {
    bar[i] = (int*)malloc(sizeof(int) * 10);
  }
  for (i = 0; i < 1000; i++) {
    free(bar[i]);
  }
  foo[6] = 123;
}


/* memory leak */
void leak_test() {
  int *foo;
  SHOW_ME;
  foo = (int*)malloc(sizeof(int) * 10);
  if (!foo) exit(0);
}

/*----------------------------------------------------------------------
 Test valgrind client requests.
 See http://valgrind.org/docs/manual/manual-core-adv.html for more
 details on valgrind client requests.
*/
void test_client_requests() {
  /* Pass a NULL callback. Valgrind will simply print the parameters. */
  /* TODO(kcc): add tests for non-trivial client requests (when they start
   * working) */
  long f = 0;
  SHOW_ME;
  VALGRIND_NON_SIMD_CALL1(f, 0xABCDEF);
  break_optimization(); /* to test an unaligned client request call */
  VALGRIND_NON_SIMD_CALL2(f, 0xBCD001, 0xBCD002);
  VALGRIND_NON_SIMD_CALL3(f, 0xCDE001, 0xCDE002, 0xCDE003);
}

/* test how valgrind prints backtraces */
NOINLINE void test_backtrace() {
  SHOW_ME;
  VALGRIND_PRINTF_BACKTRACE("BACKTRACE:\n");
  break_optimization();
}
NOINLINE void test_backtrace1() { test_backtrace(); break_optimization(); }
NOINLINE void test_backtrace2() { test_backtrace1(); break_optimization(); }
NOINLINE void test_backtrace3() { test_backtrace2(); break_optimization(); }

NOINLINE void test_printf() {
  SHOW_ME;
  VALGRIND_PRINTF("VG PRINTF\n");
  /*
  TODO(kcc): passing vargs from untrusted code to valgrind does not work yet.
  VALGRIND_PRINTF("VG PRINTF with args: %d %d %d\n", 1, 2, 3);
  */
  break_optimization();
}

/*----------------------------------------------------------------------
 Test valgrind wrappers.
 See http://valgrind.org/docs/manual/manual-core-adv.html for more
 details on function wrapping.
*/

/* Functions to wrap. */
NOINLINE int wrap_me_0() { SHOW_ME; return 123; }
NOINLINE int wrap_me_1(int a) { SHOW_ME; return a; }
NOINLINE int wrap_me_2(int a, int b) { SHOW_ME; return a+10*b; }
NOINLINE int wrap_me_3(int a, int b, int c) { SHOW_ME; return a+10*b+100*c; }

/* Wrapper functions. */

NOINLINE int I_WRAP_SONAME_FNNAME_ZZ(NaCl, wrap_me_0)() {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  SHOW_ME;
  CALL_FN_W_v(ret, fn);
  SHOW_ME;
  VALGRIND_NON_SIMD_CALL1(0, 0xABCDEF);
  return ret + 777;  /* change the return value */
}

NOINLINE int I_WRAP_SONAME_FNNAME_ZZ(NaCl, wrap_me_1)(int a) {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  SHOW_ME;
  CALL_FN_W_W(ret, fn, a);
  return ret * 7;  /* change the return value */
}

NOINLINE int I_WRAP_SONAME_FNNAME_ZZ(NaCl, wrap_me_2)(int a, int b) {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  SHOW_ME;
  CALL_FN_W_WW(ret, fn, a, b);
  return ret * 7;  /* change the return value */
}

NOINLINE int I_WRAP_SONAME_FNNAME_ZZ(NaCl, wrap_me_3)(int a, int b, int c) {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  SHOW_ME;
  CALL_FN_W_WWW(ret, fn, a, b, c);
  return ret * 7;  /* change the return value */
}

NOINLINE
void function_wrapping_test() {
  SHOW_ME;
  /* The wrappers change the return value, so if the wrappers did not work
   these asserts will fail. */
  assert(wrap_me_0() == 123 + 777);
  assert(wrap_me_1(1) == 7 * 1);
  assert(wrap_me_2(1, 2) == 7 * 21);
  assert(wrap_me_3(1, 2, 3) == 7 * 321);
}

/* test that interceptors with a large number of arguments work and do not mess
   up stack traces */
NOINLINE
int zz_wrap_me(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
  fprintf(stderr, "wrapped: %d, %d, %d, %d, %d, %d, %d\n", a1, a2, a3, a4, a5,
      a6, a7);
  VALGRIND_PRINTF_BACKTRACE("many_args BACKTRACE:\n");
  return a1 + a2 + a3 + a4 + a5 + a6 + a7;
}

NOINLINE
int I_WRAP_SONAME_FNNAME_ZZ(NaCl, zz_wrap_me)(int a1, int a2, int a3, int a4,
    int a5, int a6, int a7) {
  int ret;
  OrigFn fn;
  VALGRIND_GET_ORIG_FN(fn);
  SHOW_ME;
  CALL_FN_W_7W(ret, fn, a1, a2, a3, a4, a5, a6, a7);
  return ret * 7;  /* change the return value */
}

NOINLINE
int zz_wrap_me0(int a1, int a2, int a3, int a4, int a5, int a6, int a7) {
  int ret = zz_wrap_me(a1, a2, a3, a4, a5, a6, a7);
  break_optimization();
  return ret;
}

NOINLINE
void many_args_wrapping_test() {
  SHOW_ME;
  assert(zz_wrap_me0(1, 2, 3, 4, 5, 6, 7) == 7 * 28);
}


/* run all tests */
int main() {
  if (!RUNNING_ON_VALGRIND) {
    /* Don't run this test w/o valgrind. It would fail otherwise. */
    return 0;
  }
  /* Use poor man's gtest_filter. */
  if (1) leak_test();
  if (1) oob_read_test();
  if (1) oob_write_test();
  if (1) umr_heap_test();
  if (1) umr_stack_test();
  if (1) use_after_free_test();
  if (1) use_after_free_with_reuse_test();
  if (1) test_client_requests();
  if (1) test_backtrace3();
  if (1) test_printf();
  if (1) function_wrapping_test();
  if (1) many_args_wrapping_test();
  SHOW_ME;
  return 0;
}
