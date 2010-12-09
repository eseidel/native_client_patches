/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Windows-specific routines for verifying that Data Execution Prevention is
 * functional.
 */

#include "native_client/src/include/portability.h"

#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>

#include "native_client/src/trusted/platform_qualify/nacl_dep_qualify.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

static int g_SigFound;

static enum NaClSignalResult signal_catch(int signal,
                                          void *ctx) {
  struct NaClSignalContext sigCtx;
  int in_untrusted_code;

  NaClSignalContextFromHandler(&sigCtx, ctx);
  in_untrusted_code = NaClSignalContextIsUntrusted(&sigCtx);

  /* Should be a trusted segfault */
  if (!in_untrusted_code && 11 == signal) {
    g_SigFound = signal;
    return NACL_SIGNAL_SKIP;
  }
  return NACL_SIGNAL_SEARCH; /* some other signal we weren't expecting */
}

static int setup_signals() {
  return NaClSignalHandlerAdd(signal_catch);
}

static void restore_signals(int handlerId) {
  if (0 == NaClSignalHandlerRemove(handlerId)) {
    /* What to do if the remove failed? */
    fprintf(stderr, "Failed to unload handler.\n");
  }
}

/*
 * Returns 1 if Data Execution Prevention is present and working.
 */
int NaClAttemptToExecuteData() {
  int result;
  int handlerId;
  char *thunk_buffer = malloc(64);
  nacl_void_thunk thunk = NaClGenerateThunk(thunk_buffer, 64);

  handlerId = setup_signals();
  g_SigFound = 0;
  __try {
    thunk();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }

  /* Should be a segfault */
  if (11 == g_SigFound) {
    result = 1;
  } else {
    result = 0;
  }

  restore_signals(handlerId);
  return result;
}
