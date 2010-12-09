/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_H_

#include "native_client/src/trusted/validator_x86/types_memory_model.h"

/*
 * ncvalidate.h: exports for ncvalidate.c
 *
 * This is the library interface to the NaCl validator.
 * Basic usage:
 *   rc = NCValidateInit(base, limit, 16)
 *   if rc != 0 fail
 *   for each section:
 *     NCValidateSegment(maddr, vaddr, size);
 *   rc = NCValidateFinish();
 *   if rc != 0 fail
 * Optional reporting routines
 *   Stats_Print()
 *
 * See the README file in this directory for more info on the general
 * structure of the validator.
 */
struct NCValidatorState;

/*
 * NCValidateInit: Initialize NaCl validator internal state
 * Parameters:
 *    vbase: base virtual address for code segment
 *    vlimit: size in bytes of code segment
 *    alignment: 16 or 32, specifying alignment
 * Returns:
 *    an initialized struct NCValidatorState * if everything is okay,
 *    else NULL
 */
struct NCValidatorState *NCValidateInit(const NaClPcAddress vbase,
                                        const NaClPcAddress vlimit,
                                        const uint8_t alignment);

/*
 * Allows "stub out mode" to be enabled, in which some unsafe
 * instructions will be rendered safe by replacing them with HLT
 * instructions.
 */
void NCValidateSetStubOutMode(struct NCValidatorState *vstate,
                              int do_stub_out);

/* Validate a segment */
/* This routine will raise an segmentation exception if you ask
 * it to check memory that can't be accessed. This should of be
 * interpreted as an indication that the module in question is
 * invalid.
 */
void NCValidateSegment(uint8_t *mbase, NaClPcAddress vbase, size_t sz,
                       struct NCValidatorState *vstate);

/* Validate a segment for dynamic code replacement */
/* This routine checks that the code found at mbase_old
 * can be dynamically replaced with the code at mbase_new
 * safely.
 */
void NCValidateSegmentPair(uint8_t *mbase_old, uint8_t *mbase_new,
                           NaClPcAddress vbase, size_t sz,
                           struct NCValidatorState *vstate);

/* Check targets and alignment. Returns non-zero if there are */
/* safety issues, else returns 1                              */
/* BEWARE: vstate is invalid after this call                  */
int NCValidateFinish(struct NCValidatorState *vstate);

/* BEWARE: this call deallocates vstate.                      */
void NCValidateFreeState(struct NCValidatorState **vstate);

/* Print some interesting statistics... */
void Stats_Print(FILE *f, struct NCValidatorState *vstate);

/* Book-keeping routines called from the decoder. */
void OpcodeHisto(const uint8_t byte1,
                 struct NCValidatorState *vstate);

/* Returns the default value used for controlling printing
 * of validator messages.
 * If zero, no messages are printed.
 * If >0, only that many diagnostic errors are printed.
 * If negative, all validator diagnostics are printed.
 */
int NCValidatorGetMaxDiagnostics();

/* Changes default flag for printing validator error messages.
 * If zero, no messages are printed.
 * If >0, only that many diagnostic errors are printed.
 * If negative, all validator diagnostics are printed.
 */
void NCValidatorSetMaxDiagnostics(int new_value);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_H_ */
