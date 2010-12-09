/*
 * Copyright 2010  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <pthread.h>
#include <stdio.h>

#include <sys/nacl_syscalls.h>

int main(int argc, char **argv) {
  int rv;

  rv = chromeipc_open();
  chromeipc_close(3);

  printf("Done\n");
  return 0;
}
