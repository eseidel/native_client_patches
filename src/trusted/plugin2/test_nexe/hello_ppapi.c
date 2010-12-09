// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdio.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp.h"

PP_Module g_module_id;
PPB_GetInterface g_get_browser_interface = NULL;

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  printf("PPP_InitializeModule\n");
  // Save the global module information for later.
  g_module_id = module_id;
  g_get_browser_interface = get_browser_interface;

  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
    printf("PPP_ShutdownModule\n");
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("PPP_GetInterface\n");
  // You will normally implement a getter for at least PPP_INSTANCE_INTERFACE
  // here.
  return NULL;
}
