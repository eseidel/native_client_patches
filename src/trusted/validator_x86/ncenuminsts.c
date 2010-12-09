/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/ncenuminsts.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_illegal.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Checks that if a memory reference is a segment address, the segment is
 * one of the ignored segment registers.
 *
 * Note: This is a simplified validator test from NaClMemoryReferenceValidator
 * in file nc_memory_protect.c, pulling out the check on ignored segment
 * registers.
 */
static void NaClSafeSegmentReference(NaClValidatorState* state,
                                     NaClInstIter* iter,
                                     void* ignore) {
  uint32_t i;
  NaClInstState* inst_state = state->cur_inst_state;
  NaClExpVector* vector = state->cur_inst_vector;

  DEBUG({
      struct Gio* g = NaClLogGetGio();
      NaClLog(LOG_INFO, "Validating segments:\n");
      NaClInstStateInstPrint(g, inst_state);
      NaClInstPrint(g, state->cur_inst);
      NaClExpVectorPrint(g, vector);
    });
  /* Look for references to a segment address. */
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    NaClExp* node = &vector->node[i];
    if (state->quit) break;
    DEBUG(NaClLog(LOG_INFO, "processing argument %"NACL_PRIu32"\n", i));
    if (ExprSegmentAddress == node->kind) {
        int seg_prefix_reg_index;
        NaClOpKind seg_prefix_reg;
        DEBUG(NaClLog(LOG_INFO,
                      "found segment assign at node %"NACL_PRIu32"\n", i));

        /* Only allow if 64 bit segment addresses. */
        if (NACL_EMPTY_EFLAGS == (node->flags & NACL_EFLAG(ExprSize64))) {
          NaClValidatorInstMessage(
              LOG_ERROR, state, inst_state,
              "Assignment to non-64 bit segment address\n");
          continue;
        }
        /* Only allow segment prefix registers that are treated as
         * null prefixes.
         */
        seg_prefix_reg_index = NaClGetExpKidIndex(vector, i, 0);
        seg_prefix_reg = NaClGetExpVectorRegister(vector, seg_prefix_reg_index);
        switch (seg_prefix_reg) {
          case RegCS:
          case RegDS:
          case RegES:
          case RegSS:
            continue;
          default:
            break;
        }
        /* If reached, we don't know how to handle the segment reference. */
        NaClValidatorInstMessage(LOG_ERROR, state, inst_state,
                                 "Segment memory reference not allowed\n");
    }
  }
}

Bool NaClInstructionIsLegal(uint8_t* mbase,
                            uint8_t size,
                            NaClPcAddress vbase) {
  NaClSegment segment;
  NaClInstIter* iter;
  NaClValidatorState* state;
  Bool is_legal;
  state = NaClValidatorStateCreate(vbase, (NaClMemorySize) size, 32, RegR15);
  NaClValidatorStateInitializeValidators(state);
  NaClSegmentInitialize(mbase, vbase, (NaClMemorySize) size, &segment);
  iter = NaClInstIterCreate(&segment);
  state->cur_inst_state = NaClInstIterGetState(iter);
  state->cur_inst = NaClInstStateInst(state->cur_inst_state);
  state->cur_inst_vector = NaClInstStateExpVector(state->cur_inst_state);
  NaClValidateInstructionLegal(state, iter, NULL);
  NaClSafeSegmentReference(state, iter, NULL);
  is_legal = NaClValidatesOk(state);
  state->cur_inst_state = NULL;
  state->cur_inst = NULL;
  state->cur_inst_vector = NULL;
  NaClInstIterDestroy(iter);
  NaClValidatorStateDestroy(state);
  return is_legal;
}
