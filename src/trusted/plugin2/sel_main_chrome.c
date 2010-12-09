/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/portability.h"

#if NACL_OSX
#include <crt_externs.h>
#endif

#ifdef _WIN64  /* TODO(gregoryd): remove this when win64 issues are fixed */
#define NACL_NO_INLINE
#endif

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/expiration/expiration.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/env_cleanser.h"
#include "native_client/src/trusted/service_runtime/nacl_app.h"
#include "native_client/src/trusted/service_runtime/nacl_all_modules.h"
#include "native_client/src/trusted/service_runtime/nacl_debug.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_qualify.h"

static int const kSrpcFd = 5;

int verbosity = 0;


int NaClMainForChromium(int handle_count, const NaClHandle *handles,
                        int debug) {
  char *av[1];
  int ac = 1;
  const char **envp;
  struct NaClApp state;
  int main_thread_only = 1;
  char                          *nacl_file = "/Projects/native_client/native_client/src/trusted/plugin2/test_nexe/hello_ppapi.nexe";
  struct NaClApp *nap;
  enum NaClAbiCheckOption       check_abi = NACL_ABI_CHECK_OPTION_CHECK;
  struct GioFile                gout;
  struct GioMemoryFileSnapshot  gf;
  NaClErrorCode errcode;
  int ret_code = 1;
  struct NaClEnvCleanser env_cleanser;

#if NACL_OSX
  /* Mac dynamic libraries cannot access the environ variable directly. */
  envp = (const char **) *_NSGetEnviron();
#else
  /* Overzealous code style check is overzealous. */
  /* @IGNORE_LINES_FOR_CODE_HYGIENE[1] */
  extern char **environ;
  envp = (const char **) environ;
#endif

  NaClAllModulesInit();

  /* Add a handler to catch untrusted errors only */
  NaClSignalHandlerAdd(NaClSignalHandleUntrusted);

  /* to be passed to NaClMain, eventually... */
  av[0] = "NaClMain";

  if (!NaClAppCtor(&state)) {
    fprintf(stderr, "Error while constructing app state\n");
    goto done;
  }

  state.restrict_to_main_thread = main_thread_only;

  nap = &state;
  errcode = LOAD_OK;
  /*
   * in order to report load error to the browser plugin through the
   * secure command channel, we do not immediate jump to cleanup code
   * on error.  rather, we continue processing (assuming earlier
   * errors do not make it inappropriate) until the secure command
   * channel is set up, and then bail out.
   */

  /*
   * Ensure this operating system platform is supported.
   */
  errcode = NaClRunSelQualificationTests();
  if (LOAD_OK != errcode) {
    nap->module_load_status = errcode;
    fprintf(stderr, "Error while loading in SelMain: %s\n",
            NaClErrorString(errcode));
  }

  /* Give debuggers a well known point at which xlate_base is known.  */
  NaClGdbHook(&state);

  if (0 == GioMemoryFileSnapshotCtor(&gf, nacl_file)) {
    perror("sel_main");
    fprintf(stderr, "Cannot open \"%s\".\n", nacl_file);
    errcode = LOAD_OPEN_ERROR;
  }

  if (LOAD_OK == errcode) {
    errcode = NaClAppLoadFile((struct Gio *) &gf, nap, check_abi);
    if (LOAD_OK != errcode) {
      fprintf(stderr, "Error while loading \"%s\": %s\n",
              nacl_file,
              NaClErrorString(errcode));
      fprintf(stderr,
              ("Using the wrong type of nexe (nacl-x86-32"
               " on an x86-64 or vice versa)\n"
               "or a corrupt nexe file may be"
               " responsible for this error.\n"));
    }

    NaClXMutexLock(&nap->mu);
    nap->module_load_status = errcode;
    NaClXCondVarBroadcast(&nap->cv);
    NaClXMutexUnlock(&nap->mu);
  }

   if (LOAD_OK == errcode) {
     if (verbosity) {
       gprintf((struct Gio *) &gout, "printing NaClApp details\n");
       NaClAppPrintDetails(nap, (struct Gio *) &gout);
     }

     /*
      * Finish setting up the NaCl App.  This includes dup'ing
      * descriptors 0-2 and making them available to the NaCl App.
      */
     errcode = NaClAppPrepareToLaunch(nap,
                                      0,
                                      1,
                                      2);
     if (LOAD_OK != errcode) {
       nap->module_load_status = errcode;
       fprintf(stderr, "NaClAppPrepareToLaunch returned %d", errcode);
     }
   }

   if (-1 == (*((struct Gio *) &gf)->vtbl->Close)((struct Gio *) &gf)) {
     fprintf(stderr, "Error while closing \"%s\".\n", nacl_file);
   }
   (*((struct Gio *) &gf)->vtbl->Dtor)((struct Gio *) &gf);

 /*
  * Print out a marker for scripts to use to mark the start of app
  * output.
  */
 NaClLog(1, "NACL: Application output follows\n");

 /*
  * Make sure all the file buffers are flushed before entering
  * the application code.
  */
 fflush((FILE *) NULL);

  /*
   * Enable debugging if requested.
   */
  if (debug) NaClDebugSetAllow(1);

  NaClEnvCleanserCtor(&env_cleanser);
  if (!NaClEnvCleanserInit(&env_cleanser, envp)) {
    NaClLog(LOG_FATAL, "Failed to initialise env cleanser\n");
  }

  /*
   * only nap->ehdrs.e_entry is usable, no symbol table is
   * available.
   */
  if (!NaClCreateMainThread(nap, ac, av,
                            NaClEnvCleanserEnvironment(&env_cleanser))) {
    fprintf(stderr, "creating main thread failed\n");
    goto done;
  }

  NaClEnvCleanserDtor(&env_cleanser);

  ret_code = NaClWaitForMainThreadToExit(nap);

  /*
   * exit_group or equiv kills any still running threads while module
   * addr space is still valid.  otherwise we'd have to kill threads
   * before we clean up the address space.
   */
  return ret_code;

 done:
  fflush(stdout);

  NaClAllModulesFini();

  return ret_code;
}
