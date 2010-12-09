# Copyright (c) 2010 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../build/common.gypi',
  ],
  'targets': [
    {
      'target_name': 'nacl_ppapi_browser',
      'type': 'static_library',
      'sources': [
        'utility.cc',
        'browser_core.cc',
        'browser_completion_callback_invoker.cc',
        'browser_globals.cc',
        'browser_graphics_2d.cc',
        'browser_image_data.cc',
        'browser_instance.cc',
        'browser_ppp.cc',
        'browser_upcall.cc',
        'object.cc',
        'object_proxy.cc',
        'object_serialize.cc',
        'objectstub_rpc_impl.cc',
        # Autogerated files
        'ppp_rpc_client.cc',
        'ppb_rpc_server.cc',
        'upcall_server.cc',
      ],
      'include_dirs': [
        '<(DEPTH)/native_client/src/shared/ppapi_proxy/trusted',
      ],
    },
  ],
}
