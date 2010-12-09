/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime.  I/O Descriptor / Handle abstraction.
 * Bound IMC descriptors.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"

#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"


static struct NaClDescVtbl const kNaClDescImcBoundDescVtbl;  /* fwd */

int NaClDescImcBoundDescCtor(struct NaClDescImcBoundDesc  *self,
                             NaClHandle                   h) {
  struct NaClDesc *basep = (struct NaClDesc *) self;

  basep->base.vtbl = (struct NaClRefCountVtbl const *) NULL;
  if (!NaClDescCtor(basep)) {
    return 0;
  }
  self->h = h;
  basep->base.vtbl =
      (struct NaClRefCountVtbl const *) &kNaClDescImcBoundDescVtbl;
  return 1;
}

void NaClDescImcBoundDescDtor(struct NaClRefCount *vself) {
  struct NaClDescImcBoundDesc *self = (struct NaClDescImcBoundDesc *) vself;

  NaClClose(self->h);
  self->h = NACL_INVALID_HANDLE;
  vself->vtbl = (struct NaClRefCountVtbl const *) &kNaClDescVtbl;
  (*vself->vtbl->Dtor)(vself);
}

int NaClDescImcBoundDescFstat(struct NaClDesc          *vself,
                              struct nacl_abi_stat     *statbuf) {
  UNREFERENCED_PARAMETER(vself);

  memset(statbuf, 0, sizeof *statbuf);
  statbuf->nacl_abi_st_mode = NACL_ABI_S_IFBOUNDSOCK;
  return 0;
}

int NaClDescImcBoundDescAcceptConn(struct NaClDesc *vself,
                                   struct NaClDesc **result) {
  /*
   * See NaClDescConnCapConnectAddr code in nacl_desc_conn_cap.c
   */
  struct NaClDescImcBoundDesc *self = (struct NaClDescImcBoundDesc *) vself;
  int                         retval;
  NaClHandle                  received_fd;
  struct NaClDescImcDesc      *peer;
  struct iovec                iovec;
  struct msghdr               accept_msg;
  struct cmsghdr              *cmsg;
  char                        data_buf[1];
  char                        control_buf[CMSG_SPACE(sizeof(int))];
  int                         received;

  peer = malloc(sizeof(*peer));
  if (peer == NULL) {
    return -NACL_ABI_ENOMEM;
  }

  iovec.iov_base = data_buf;
  iovec.iov_len = sizeof(data_buf);
  accept_msg.msg_iov = &iovec;
  accept_msg.msg_iovlen = 1;
  accept_msg.msg_name = NULL;
  accept_msg.msg_namelen = 0;
  accept_msg.msg_control = control_buf;
  accept_msg.msg_controllen = sizeof(control_buf);

  NaClLog(3,
          ("NaClDescImcBoundDescAcceptConn(0x%08"NACL_PRIxPTR", "
           " h = %d\n"),
          (uintptr_t) vself,
          self->h);

  received = recvmsg(self->h, &accept_msg, 0);
  if (received != 1 || data_buf[0] != 'c') {
    NaClLog(LOG_ERROR,
            ("NaClDescImcBoundDescAcceptConn:"
             " could not receive connection message, errno %d\n"),
            errno);
    return -NACL_ABI_EIO;
  }

  received_fd = -1;
  for (cmsg = CMSG_FIRSTHDR(&accept_msg);
       cmsg != NULL;
       cmsg = CMSG_NXTHDR(&accept_msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS &&
        cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
      if (-1 != received_fd) {
        NaClLog(LOG_ERROR, ("NaClDescImcBoundDescAcceptConn: connection"
                            " message contains more than 1 descriptors?!?\n"));
        return -NACL_ABI_EIO;
      }
      /* We use memcpy() rather than assignment through a cast to avoid
         strict-aliasing warnings */
      memcpy(&received_fd, CMSG_DATA(cmsg), sizeof(int));
    }
  }
  if (received_fd == -1) {
    NaClLog(LOG_ERROR, ("NaClDescImcBoundDescAcceptConn: connection"
                        " message contains NO descriptors?!?\n"));
    return -NACL_ABI_EIO;
  }

  if (!NaClDescImcDescCtor(peer, received_fd)) {
    retval = -NACL_ABI_EMFILE;
    goto cleanup;
  }
  received_fd = NACL_INVALID_HANDLE;

  *result = (struct NaClDesc *) peer;
  retval = 0;

cleanup:
  if (retval < 0) {
    if (received_fd != NACL_INVALID_HANDLE) {
      NaClClose(received_fd);
    }
    free(peer);
  }
  return retval;
}

static struct NaClDescVtbl const kNaClDescImcBoundDescVtbl = {
  {
    NaClDescImcBoundDescDtor,
  },
  NaClDescMapNotImplemented,
  NaClDescUnmapUnsafeNotImplemented,
  NaClDescUnmapNotImplemented,
  NaClDescReadNotImplemented,
  NaClDescWriteNotImplemented,
  NaClDescSeekNotImplemented,
  NaClDescIoctlNotImplemented,
  NaClDescImcBoundDescFstat,
  NaClDescGetdentsNotImplemented,
  NACL_DESC_BOUND_SOCKET,
  NaClDescExternalizeSizeNotImplemented,
  NaClDescExternalizeNotImplemented,
  NaClDescLockNotImplemented,
  NaClDescTryLockNotImplemented,
  NaClDescUnlockNotImplemented,
  NaClDescWaitNotImplemented,
  NaClDescTimedWaitAbsNotImplemented,
  NaClDescSignalNotImplemented,
  NaClDescBroadcastNotImplemented,
  NaClDescSendMsgNotImplemented,
  NaClDescRecvMsgNotImplemented,
  NaClDescConnectAddrNotImplemented,
  NaClDescImcBoundDescAcceptConn,
  NaClDescPostNotImplemented,
  NaClDescSemWaitNotImplemented,
  NaClDescGetValueNotImplemented,
};
