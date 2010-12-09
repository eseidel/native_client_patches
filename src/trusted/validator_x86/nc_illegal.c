/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* Validator to check that instructions are in the legal subset. */

#include "native_client/src/trusted/validator_x86/nc_illegal.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Message to use if we don't have a more precise reason why the instruction
 * is illegal.
 */
static const char* kNaClGenericReasonDisallowed =
    "Illegal native client instruction";

/* This function returns a printable description of the reason an instruction
 * is disallowed, or NULL if the reason is not understood.
 */
static const char* NaClReasonWhyDisallowed(NaClDisallowsFlag flag) {
  switch (flag) {
    case NaClTooManyPrefixBytes:
      return "More than one (non-REX) prefix byte specified";
    case NaClMarkedIllegal:
      return "This instruction has been marked illegal by Native Client";
    case NaClMarkedInvalid:
      return "Opcode sequence doesn't define a valid x86 instruction";
    case NaClMarkedSystem:
      return "System instructions are not allowed by Native Client";
    case NaClHasBadSegmentPrefix:
      return "Uses a segment prefix byte not allowed by Native Client";
    case NaClCantUsePrefix67:
      return "Use of 67 (ADDR16) prefix not allowed by Native Client";
    case NaClMultipleRexPrefix:
      return "Multiple use of REX prefix not allowed";
    case NaClRepDisallowed:
      return
          "Use of REP (F3) prefix for instruction not allowed by "
          "Native Client";
    case NaClRepneDisallowed:
      return
          "Use of REPNE (F2) prefix for instruction not allowed by "
          "Native Client";
    case NaClData16Disallowed:
      return
          "Use of DATA16 (66) prefix for instruction not allowed by "
          "Native Client";
    default:
      return NULL;
  }
}

/* Same as NaClReasonWhyDisallowed, except that the unknown value (NULL)
 * is replaced by a useful print error message.
 */
static const char* NaClGetReasonWhyDisallowed(NaClDisallowsFlag flag) {
  const char* why = NaClReasonWhyDisallowed(flag);
  if (NULL == why) {
    /* If reached, we don't understand the flag. Non the less, we should
     * return a message explaining that the instruction is illegal, so
     * that an appropriate error message can be printed.
     */
    why = kNaClGenericReasonDisallowed;
  }
  return why;
}

/* This function checks properties of prefix bytes on an instruction,
 * reporting issues not allowed by native client.
 *
 * Parameters are:
 *    state : The current validator state to check.
 *    is_legal: Flag set to FALSE if any prefix problems are found.
 *    disallows_flags : Set of flags, describing the reason(s) the
 *              current instruction in the validator state is not
 *              legal in native client. Updated as appropriate.
 */
static void NaClCheckForPrefixIssues(NaClValidatorState* state,
                                     Bool* is_legal,
                                     NaClDisallowsFlags* disallows_flags) {
  NaClInstState* inst_state = state->cur_inst_state;
  NaClInst* inst = state->cur_inst;
  /* Don't allow more than one (non-REX) prefix. */
  int num_prefix_bytes = inst_state->num_prefix_bytes;
  if (inst_state->rexprefix) --num_prefix_bytes;

  /* Don't allow an invalid instruction. */
  if (!NaClInstStateIsValid(inst_state)) {
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedInvalid);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClMarkedInvalid)));
  }

  /* Don't allow multiple prefix bytes, except for the special
   * case of the pair DATA16 and LOCK (allowed so that one can
   * lock short integers).
   *
   * Note: Explicit NOP sequences that use multiple 66 values are
   * recognized as special cases, and need not be processed here.
   */
  if (num_prefix_bytes > 1) {
    /* Allow data prefix if lock prefix also given. */
    if ((num_prefix_bytes == 2) &&
        (inst_state->prefix_mask & kPrefixDATA16) &&
        (inst_state->prefix_mask & kPrefixLOCK)) {
      /* Allow special case. */
    } else {
      *is_legal = FALSE;
      *disallows_flags |= NACL_DISALLOWS_FLAG(NaClTooManyPrefixBytes);
      DEBUG(NaClLog(LOG_INFO, "%s\n",
                    NaClGetReasonWhyDisallowed(NaClTooManyPrefixBytes)));
    }
  }

  /* Only allow REP/REPE/REPZ (F3) prefix if flag specifies it is allowed. */
  if ((inst_state->prefix_mask & kPrefixREP) &&
      (NACL_EMPTY_IFLAGS == (inst->flags & NACL_IFLAG(OpcodeAllowsRep)))) {
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClRepDisallowed);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClRepDisallowed)));
  }

  /* Only allow REPNE/REPNZ (F2) prefix if flag specifies it is allowed. */
  if ((inst_state->prefix_mask & kPrefixREPNE) &&
      (NACL_EMPTY_IFLAGS == (inst->flags & NACL_IFLAG(OpcodeAllowsRepne)))) {
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClRepneDisallowed);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClRepneDisallowed)));
  }

  /* Only allow Data16 (66) prefix if flag specifies it is allowed. */
  if ((inst_state->prefix_mask & kPrefixDATA16) &&
      (NACL_EMPTY_IFLAGS == (inst->flags & NACL_IFLAG(OpcodeAllowsData16)))) {
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClData16Disallowed);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClData16Disallowed)));
  }

  /* Don't allow more than one REX prefix. */
  if (inst_state->num_rex_prefixes > 1) {
    /* NOTE: does not apply to NOP, since they are parsed using
     * special handling (i.e. explicit byte sequence matches) that
     * doesn't explicitly define prefix bytes.
     *
     * NOTE: We don't disallow this while decoding, since xed doesn't
     * disallow this, and we want to be able to compare our tool
     * to xed.
     */
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClMultipleRexPrefix);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClMultipleRexPrefix)));
  }

  /* Don't allow shortened address sizes. That is prefix ADDR16. */
  if (inst_state->prefix_mask & kPrefixADDR16) {
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClCantUsePrefix67);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClCantUsePrefix67)));
  }

  /* Don't allow CS, DS, ES, or SS prefix overrides in 64-bit mode,
   * since it has no effect, unless explicitly stated
   * otherwise.
   */
  if (NACL_TARGET_SUBARCH == 64) {
    if (inst_state->prefix_mask &
        (kPrefixSEGCS | kPrefixSEGSS | kPrefixSEGES |
         kPrefixSEGDS)) {
      if (NACL_EMPTY_IFLAGS ==
          (inst->flags & NACL_IFLAG(IgnorePrefixSEGCS))) {
        *is_legal = FALSE;
        *disallows_flags |= NACL_DISALLOWS_FLAG(NaClHasBadSegmentPrefix);
        DEBUG(NaClLog(LOG_INFO, "%s\n",
                      NaClGetReasonWhyDisallowed(NaClHasBadSegmentPrefix)));
      }
    }
  }

}

/* Checks instruction details of the current instruction to see if there
 * are any red flags that make the instruction illegal.
 *
 * Parameters are:
 *    state : The current validator state to check.
 *    is_legal: Flag set to FALSE if any prefix problems are found.
 *    disallows_flags : Set of flags, describing the reason(s) the
 *              current instruction in the validator state is not
 *              legal in native client. Updated as appropriate.
 */
static void NaClCheckIfMarkedIllegal(NaClValidatorState* state,
                                     Bool* is_legal,
                                     NaClDisallowsFlags* disallows_flags) {
  if (NACL_IFLAG(NaClIllegal) & state->cur_inst->flags) {
    *is_legal = FALSE;
    *disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedIllegal);
    DEBUG(NaClLog(LOG_INFO, "%s\n",
                  NaClGetReasonWhyDisallowed(NaClMarkedIllegal)));
  }

  /* Check other forms to disallow.
   * TODO(karl): Move these checks into the model generator, setting the
   * NaClIllegal flag. Once that is completed, this switch statement can
   * can be removed.
   */
  switch (state->cur_inst->insttype) {
    case NACLi_RETURN:
    case NACLi_EMMX:
      /* EMMX needs to be supported someday but isn't ready yet. */
    case NACLi_ILLEGAL:
    case NACLi_SYSTEM:
    case NACLi_RDMSR:
    case NACLi_RDTSCP:
    case NACLi_LONGMODE:
    case NACLi_SVM:
    case NACLi_3BYTE:
    case NACLi_CMPXCHG16B:
    case NACLi_UNDEFINED:
      *is_legal = FALSE;
      *disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedIllegal);
      DEBUG(NaClLog(LOG_INFO, "%s\n",
                    NaClGetReasonWhyDisallowed(NaClMarkedIllegal)));
      break;
    case NACLi_INVALID:
      *is_legal = FALSE;
      *disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedInvalid);
      DEBUG(NaClLog(LOG_INFO, "%s\n",
                    NaClGetReasonWhyDisallowed(NaClMarkedInvalid)));
      break;
    case NACLi_SYSCALL:
    case NACLi_SYSENTER:
      *is_legal = FALSE;
      *disallows_flags |= NACL_DISALLOWS_FLAG(NaClMarkedSystem);
      DEBUG(NaClLog(LOG_INFO, "%s\n",
                    NaClGetReasonWhyDisallowed(NaClMarkedSystem)));
      break;
    default:
      break;
  }
}

/* Prints out error messages describing why the current instruction
 * in the given validator state is not legal in native client.
 *
 * Parameters are:
 *    state : The current validator state to check.
 *    disallows_flags : Set of flags, describing the reason(s) the
 *              current instruction in the validator state is not
 *              legal in native client. Updated as appropriate.
 */
static void NaClReportWhyNaClIllegal(NaClValidatorState* state,
                                     NaClDisallowsFlags disallows_flags) {
  if (NACL_EMPTY_DISALLOWS_FLAGS != disallows_flags) {
    int i;
    Bool printed_reason = FALSE;
    NaClInstState* inst_state = state->cur_inst_state;
    /* Print out error message for each reason the instruction is disallowed. */
    for (i = 0; i < NaClDisallowsFlagEnumSize; ++i) {
      if (disallows_flags & NACL_DISALLOWS_FLAG(i)) {
        const char* why_disallowed = NaClReasonWhyDisallowed(i);
        if (NULL != why_disallowed) {
          printed_reason = TRUE;
          NaClValidatorInstMessage(
              LOG_ERROR, state, inst_state, "%s\n", why_disallowed);
        }
      }
      /* Stop looking if we should quit reporting errors. */
      if (state->quit) break;
    }
    /* Be sure we print a reason (in case the switch isn't complete). */
    if (!printed_reason) {
      disallows_flags = NACL_EMPTY_DISALLOWS_FLAGS;
    }
  }
  if (NACL_EMPTY_DISALLOWS_FLAGS == disallows_flags) {
    /* No known reason was recorded, but the instruction was recorded as illegal.
     * Report that the instruction is not acceptable.
     */
    NaClValidatorInstMessage(LOG_ERROR, state, state->cur_inst_state,
                             "%s\n", kNaClGenericReasonDisallowed);
  }
}

void NaClValidateInstructionLegal(NaClValidatorState* state,
                                  NaClInstIter* iter,
                                  void* ignore) {
  Bool is_legal = TRUE;
  NaClDisallowsFlags disallows_flags = NACL_EMPTY_DISALLOWS_FLAGS;
  DEBUG({
      struct Gio* g = NaClLogGetGio();
      NaClLog(LOG_INFO, "->NaClValidateInstructionLegal\n");
      NaClInstPrint(g, NaClInstStateInst(state->cur_inst_state));
      NaClExpVectorPrint(g, state->cur_inst_vector);
    });
  NaClCheckForPrefixIssues(state, &is_legal, &disallows_flags);
  NaClCheckIfMarkedIllegal(state, &is_legal, &disallows_flags);
  if (!is_legal) {
    NaClReportWhyNaClIllegal(state, disallows_flags);
  }
  DEBUG(NaClLog(LOG_INFO,
                "<-NaClValidateInstructionLegal: is_legal = %"NACL_PRIdBool"\n",
                is_legal));
}
