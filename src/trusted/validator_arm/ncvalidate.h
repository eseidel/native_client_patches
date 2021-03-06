/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 * Copyright 2009, Google Inc.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_NCVALIDATE_H
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_NCVALIDATE_H

/*
 * The C interface to the validator, for use by the sel_ldr (which isn't
 * C++).  This provides a restricted version of the validation process,
 * without detailed user feedback, etc.
 *
 * This header file must be valid C and C++!
 */

#include <stdint.h>
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

/*
 * Validates a complete code segment.
 * Arguments:
 *   mbase  location of the code in memory right now.
 *   vbase  virtual address where the code will appear at runtime.
 *   size   number of bytes of code provided.
 * Result: 0 if validation succeeded, non-zero if we found problems.
 */
int NCValidateSegment(uint8_t *mbase, uint32_t vbase, size_t size);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_ARM_V2_NCVALIDATE_H
