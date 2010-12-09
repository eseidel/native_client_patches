/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// A collection of debugging related interfaces.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_

#include <setjmp.h>
#include <signal.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#define SRPC_PLUGIN_DEBUG 1

namespace plugin {

// CatchSignals encapsulates the setup and removal of application-defined
// signal handlers.
// TODO(sehr): need to revisit thread-specific, etc., signal handling

class ScopedCatchSignals {
 public:
  typedef void (*SigHandlerType)(int signal_number);
  // Installs handler as the signal handler for SIGPIPE.
  explicit ScopedCatchSignals(SigHandlerType handler) {
#ifndef _MSC_VER
    prev_pipe_handler_ = signal(SIGPIPE, handler);
#endif
  }
  // Restores the previous signal handler
  ~ScopedCatchSignals() {
#ifndef _MSC_VER
    signal(SIGPIPE, prev_pipe_handler_);
#endif
  }

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(ScopedCatchSignals);
#ifndef _MSC_VER
  SigHandlerType prev_pipe_handler_;
#endif
};

// Tests that a string is a valid JavaScript identifier.  According to the
// ECMAScript spec, this should be done in terms of unicode character
// categories.  For now, we are simply limiting identifiers to the ASCII
// subset of that spec.  If successful, it returns the length of the
// identifier in the location pointed to by length (if it is not NULL).
// TODO(sehr): add Unicode identifier support.
bool IsValidIdentifierString(const char* strval, uint32_t* length);

// Platform abstraction for setjmp and longjmp
// TODO(sehr): move to portability.h
#ifdef _MSC_VER
#define PLUGIN_SETJMP(buf, mask)   setjmp(buf)
#define PLUGIN_LONGJMP(buf, value) longjmp(buf, value)
#define PLUGIN_JMPBUF jmp_buf
#else
#define PLUGIN_SETJMP(buf, mask)   sigsetjmp(buf, mask)
#define PLUGIN_LONGJMP(buf, value) siglongjmp(buf, value)
#define PLUGIN_JMPBUF sigjmp_buf
#endif  // _MSC_VER

// Debugging print utility
extern int gNaClPluginDebugPrintEnabled;
extern int NaClPluginDebugPrintCheckEnv();
#if SRPC_PLUGIN_DEBUG
#  define PLUGIN_PRINTF(args) do {                                    \
    if (-1 == ::plugin::gNaClPluginDebugPrintEnabled) {               \
      ::plugin::gNaClPluginDebugPrintEnabled =                        \
          ::plugin::NaClPluginDebugPrintCheckEnv();                   \
    }                                                                 \
    if (0 != ::plugin::gNaClPluginDebugPrintEnabled) {                \
      printf("%08"NACL_PRIx32": ", NaClThreadId());                    \
      printf args;                                                    \
      fflush(stdout);                                                 \
    }                                                                 \
  } while (0)
#else
#  define PLUGIN_PRINTF(args) do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_
