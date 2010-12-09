/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime threads abstraction layer.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_

/*
 * We cannot include this header file from an installation that does not
 * have the native_client source tree.
 * TODO(sehr): use export_header.py to copy these files out.
 */
#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

#if NACL_LINUX || NACL_OSX || defined(__native_client__)
# include "native_client/src/shared/platform/linux/nacl_threads_types.h"
#elif NACL_WINDOWS
/*
 * Needed for WINAPI.
 */
# include "native_client/src/shared/platform/win/nacl_threads_types.h"
#else
# error "thread abstraction not defined for target OS"
#endif

/*
 * We provide two simple, portable thread creation interfaces:
 *
 * Joinable threads, created with NaClThreadCreateJoinable().  These
 * must eventually be waited for using NaClThreadJoin() or the thread
 * handle will leak.
 *
 * Non-joinable (detached) threads, created with NaClThreadCtor().
 * These cannot be waited for.
 * TODO(mseaborn): The corresponding NaClThreadDtor() needs to be
 * called at some point to prevent a handle leak, but it should
 * probably be merged into NaClThreadCtor().  Calling it immediately
 * after NaClThreadCtor() should be OK.
 */

int NaClThreadCtor(struct NaClThread  *ntp,
                   void               (WINAPI *start_fn)(void *),
                   void               *state,
                   size_t             stack_size);
void NaClThreadDtor(struct NaClThread *ntp);

int NaClThreadCreateJoinable(struct NaClThread  *ntp,
                             void               (WINAPI *start_fn)(void *),
                             void               *state,
                             size_t             stack_size);
void NaClThreadJoin(struct NaClThread *ntp);

/*
 * NaClThreadExit is invoked by the thread itself.
 */
void NaClThreadExit(void);

/*
 * NaClThreadKill will be used to attempt to clean up after a badly
 * behaving NaClApp.
 */
void NaClThreadKill(struct NaClThread *target);

uint32_t NaClThreadId(void);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_NACL_THREADS_H_ */
