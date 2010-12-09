/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <stdlib.h>
#include <nacl/nacl_srpc.h>
#include <sys/mman.h>
#include <sys/stat.h>

#ifdef __GLIBC__
/*
 * TODO(mseaborn): Add a NaCl-specific header, separate from
 * <sys/stat.h>, that defines this.
 */
# define S_IFSHM 0000240000
#endif

void Cat(NaClSrpcRpc *rpc,
         NaClSrpcArg **in_args,
         NaClSrpcArg **out_args,
         NaClSrpcClosure *done) {
  int         fd;
  struct stat stb;
  int         ch;
  int         nchar;
  int         bufsize;

  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  fd = in_args[0]->u.hval;
  printf("CatFile: Got fd %d\n", fd);
  if (-1 != fstat(fd, &stb)) {
#define P(fmt, field) \
    do { printf(#field " = " fmt "\n", (int) stb.field); } while (0)

    P("0x%04x", st_mode);
    P("0x%x", st_nlink);
    P("0x%x", st_size);
    P("%d", st_size);
#undef P
  }
  bufsize = out_args[0]->u.count;
  printf("read loop, up to %d chars\n", bufsize);
  if ((stb.st_mode & S_IFMT) == S_IFSHM) {
    /* Chrome integration returns a shared memory descriptor for this now. */
    char *file_map;
    printf("mmapping\n");
    file_map = (char *) mmap(NULL, stb.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (MAP_FAILED == file_map) {
      printf("map failed");
      done->Run(done);
      return;
    }
    for (nchar = 0; nchar < bufsize - 1; ++nchar) {
      ch = file_map[nchar];
      out_args[0]->arrays.carr[nchar] = ch;
      putchar(ch);
    }
    out_args[0]->arrays.carr[nchar] = '\0';
    printf("EOF\n");
  } else {
    FILE *iob = fdopen(fd, "r");
    printf("fdopening\n");
    if (NULL == iob) {
      printf("fdopen failed");
      done->Run(done);
      return;
    }
    for (nchar = 0; EOF != (ch = getc(iob)) && nchar < bufsize-1; ++nchar) {
      out_args[0]->arrays.carr[nchar] = ch;
      putchar(ch);
    }
    out_args[0]->arrays.carr[nchar] = '\0';
    printf("EOF\n");
    fclose(iob);
  }
  printf("got %d bytes\n", nchar);
  printf("out param: %.*s\n", nchar, out_args[0]->arrays.carr);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "cat:h:C", Cat },
  { NULL, NULL },
};

int main() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
