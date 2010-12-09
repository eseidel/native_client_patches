/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * nc_jumps.c - Validate where valid jumps can occur.
 */

#include <assert.h>
#include <stdlib.h>

#include "native_client/src/trusted/validator_x86/nc_jumps.h"

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/validator_x86/nc_inst_trans.h"
#include "native_client/src/trusted/validator_x86/ncop_exps.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter.h"
#include "native_client/src/trusted/validator_x86/ncvalidate_iter_internal.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Models a set of addresses using an an array of possible addresses,
 * where the last 3 bits are unioned together using a bit mask. This cuts
 * down on the memory footprint of the address table.
 */
typedef uint8_t* NaClAddressSet;

/* Model the set of possible 3-bit tails of possible PcAddresses. */
static const uint8_t nacl_pc_address_masks[8] = {
  0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };

/* Model the offset created by removing the bottom three bits of a PcAddress. */
typedef NaClPcAddress NaClPcOffset;

/* Convert an address into the corresponding offset in an address table.
 * That is, strip off the last three bits, since these remaining bits
 * will be encoded using the union of address masks in the address table.
 */
static INLINE NaClPcOffset NaClPcAddressToOffset(NaClPcAddress address) {
  return address >> 3;
}

/* Convert the 3 lower bits of an address into the corresponding address mask
 * to use.
 */
static INLINE uint8_t NaClPcAddressToMask(NaClPcAddress address) {
  return nacl_pc_address_masks[(int) (address & (NaClPcAddress)0x7)];
}

static Bool NaClCheckAddressRange(NaClPcAddress address,
                                  NaClValidatorState* state) {
  if (address < state->vbase) {
    NaClValidatorPcAddressMessage(LOG_ERROR, state, address,
                                  "Jump to address before code block.\n");
    return FALSE;
  }
  if (address >= state->vlimit) {
    NaClValidatorPcAddressMessage(LOG_ERROR, state, address,
                                  "Jump to address beyond code block limit.\n");
    return FALSE;
  }
  return TRUE;
}

Bool NACL_FLAGS_identity_mask = FALSE;

static INLINE uint8_t NaClGetJumpMask(NaClValidatorState* state) {
  return NACL_FLAGS_identity_mask
      ? (uint8_t) 0xFF
      : (uint8_t) (~state->alignment_mask);
}

/* Return true if the corresponding address is in the given address set. */
static INLINE uint8_t NaClAddressSetContains(NaClAddressSet set,
                                             NaClPcAddress address,
                                             NaClValidatorState* state) {
  if (NaClCheckAddressRange(address, state)) {
    NaClPcAddress offset = address - state->vbase;
    return set[NaClPcAddressToOffset(offset)] & NaClPcAddressToMask(offset);
  } else {
    return FALSE;
  }
}

/* Adds the given address to the given address set. */
static void NaClAddressSetAdd(NaClAddressSet set, NaClPcAddress address,
                              NaClValidatorState* state) {
  if (NaClCheckAddressRange(address, state)) {
    NaClPcAddress offset = address - state->vbase;
    DEBUG(NaClLog(LOG_INFO,
                  "Address set add: %"NACL_PRIxNaClPcAddress"\n", address));
    set[NaClPcAddressToOffset(offset)] |= NaClPcAddressToMask(offset);
  }
}

/* Returns the array size to use for the given memory size. */
static size_t NaClAddressSetArraySize(NaClMemorySize size) {
  /* Be sure to add an element for partial overlaps. */
  /* TODO(karl) The cast to size_t for the number of elements may
   * cause loss of data. We need to fix this. This is a security
   * issue when doing cross-platform (32-64 bit) generation.
   */
  return (size_t) NaClPcAddressToOffset(size) + 1;
}

/* Create an address set for the range 0..Size. */
static NaClAddressSet NaClAddressSetCreate(NaClMemorySize size) {
  return (NaClAddressSet) calloc(NaClAddressSetArraySize(size),
                                 sizeof(uint8_t));
}

/* frees the memory of the address set back to the system. */
static INLINE void NaClAddressSetDestroy(NaClAddressSet set) {
  free(set);
}

/* Holds information collected about each instruction, and the
 * targets of possible jumps. Then, after the code has been processed,
 * this information is processed to see if there are any invalid jumps.
 */
typedef struct NaClJumpSets {
  /* Holds the set of possible target addresses that can be the result of
   * a jump.
   */
  NaClAddressSet actual_targets;
  /* Holds the set of valid instruction entry points (whenever a pattern of
   * multiple instructions are used, the sequence will be treated as atomic,
   * only having the first address in the set).
   */
  NaClAddressSet possible_targets;
  /* Removed targets, due to instruction being in the middle of an atomic
   * sequence. Note: This is needed so that we can allow validators to
   * run in any order. If we didn't do this, then we are very timing dependent
   * on calls to NaClMarkInstructionJumpIllegal, which must appear after
   * the call to NaClJumpValidator.
   */
  NaClAddressSet removed_targets;
  /* Holds the (array) size of each set above. */
  size_t set_array_size;
} NaClJumpSets;

/* Generates a jump validator. */
NaClJumpSets* NaClJumpValidatorCreate(NaClValidatorState* state) {
  NaClPcAddress align_base = state->vbase & (~state->alignment);
  NaClJumpSets* jump_sets = (NaClJumpSets*) malloc(sizeof(NaClJumpSets));
  if (jump_sets != NULL) {
    jump_sets->actual_targets =
        NaClAddressSetCreate(state->vlimit - align_base);
    jump_sets->possible_targets =
        NaClAddressSetCreate(state->vlimit - align_base);
    jump_sets->removed_targets =
        NaClAddressSetCreate(state->vlimit - align_base);
    if (jump_sets->actual_targets == NULL ||
        jump_sets->possible_targets == NULL ||
        jump_sets->removed_targets == NULL) {
      NaClValidatorMessage(LOG_FATAL, state, "unable to allocate jump sets");
    }
    jump_sets->set_array_size =
        NaClAddressSetArraySize(state->vlimit - align_base);
  }
  return jump_sets;
}

/* Record that there is an explicit jump from the from_address to the
 * to_address, for the validation defined by the validator state.
 * Parameters:
 *   state - The state of the validator.
 *   from_address - The address of the instruction that does the jump.
 *   to_address - The address that the instruction jumps to.
 *   jump_sets - The collected information on (explicit) jumps.
 *   inst - The instruction that does the jump.
 */
static void NaClAddJumpToJumpSets(NaClValidatorState* state,
                                  NaClPcAddress from_address,
                                  NaClPcAddress to_address,
                                  NaClJumpSets* jump_sets,
                                  NaClInstState* inst) {
  /* If the address is between state->vbase and state->vlimit, assume
   * good (unless we later find it jumping into a pseudo instruction).
   * Otherwise, only allow if 0 mod 32.
   */
  DEBUG(NaClLog(LOG_INFO, "Add jump to jump sets: %"
                NACL_PRIxNaClPcAddress" -> %"NACL_PRIxNaClPcAddress"\n",
                from_address, to_address));
  if (state->vbase <= to_address && to_address < state->vlimit) {
    /* Remember address for checking later. */
    DEBUG(NaClLog(LOG_INFO, "Add jump to target: %"NACL_PRIxNaClPcAddress
                  " -> %"NACL_PRIxNaClPcAddress"\n",
                  from_address, to_address));
    NaClAddressSetAdd(jump_sets->actual_targets, to_address, state);
  }
  /* The range check may not be strictly necessary given that we have
   * guard regions around the sandbox address space, but it shouldn't
   * hurt to disallow branches that overflow or underflow the address
   * space.
   */
  else if ((to_address & state->alignment_mask) == 0 &&
           (to_address & ~(NaClPcAddress) 0xffffffff) == 0) {
    /* Allow bundle-aligned jump. */
  }
  else {
    NaClValidatorInstMessage(LOG_ERROR, state, inst,
                             "Instruction jumps to bad address\n");
  }
}

static Bool NaClExtractBinaryOperandIndices(
    NaClInstState* inst,
    int* op_1,
    int* op_2) {
  uint32_t index;
  NaClExpVector* nodes = NaClInstStateExpVector(inst);
  *op_1 = -1;
  *op_2 = -1;

  for (index = 0; index < nodes->number_expr_nodes; ++index) {
    if (OperandReference == nodes->node[index].kind) {
      if (-1 == *op_1) {
        *op_1 = index + 1;
      } else {
        *op_2 = index + 1;
        return TRUE;
      }
    }
  }
  return FALSE;
}

/* Given the index of a memory offset in the given expression vector, return
 * true iff the memory offset is of the form [base+index].
 *
 * Parameters:
 *   vector - The expression vector for the instruction being examined.
 *   index - The index into the expression vector where the memory offset
 *           occurs.
 *   base_register - The base register expected in the memory offset.
 *   index_register - The index register expected in the memory offset.
 */
static Bool NaClMemOffsetMatchesBaseIndex(NaClExpVector* vector,
                                          int memoffset_index,
                                          NaClOpKind base_register,
                                          NaClOpKind index_register) {
  int r1_index = memoffset_index + 1;
  int r2_index = r1_index + NaClExpWidth(vector, r1_index);
  int scale_index = r2_index + NaClExpWidth(vector, r2_index);
  int disp_index = scale_index + NaClExpWidth(vector, scale_index);
  NaClOpKind r1 = NaClGetExpVectorRegister(vector, r1_index);
  NaClOpKind r2 = NaClGetExpVectorRegister(vector, r2_index);
  uint64_t scale = NaClGetExpConstant(vector, scale_index);
  uint64_t disp = NaClGetExpConstant(vector, disp_index);
  assert(ExprMemOffset == vector->node[memoffset_index].kind);
  return r1 == base_register &&
      r2 == index_register &&
      1 == scale &&
      0 == disp;
}

/* Checks if an indirect jump (in 64-bit mode) is native client compliant.
 *
 * Expects pattern:
 *    and %REG32-A, MASK
 *    lea %REG64-B, [%RBASE + %REG64-A]
 *    jmp %REG64-B
 *
 * Or:
 *    and %REG32-A, MASK
 *    add %REG64-A, %RBASE
 *    jmp %REG64-A
 *
 * where MASK is all 1/s except for the alignment mask bits, which must be zero.
 *
 * REG32 is the corresponding 32-bit register that whose value will get zero
 * extended by the AND operation into the corresponding 64-bit register REG64.
 *
 * It is assumed that opcode 0x83 is used for the AND operation, and hence, the
 * mask is a single byte.
 *
 * Note: applies to all kinds of jumps and calls.
 *
 * Parameters:
 *   state - The state of the validator.
 *   iter - The instruction iterator being used.
 *   inst_pc - The (virtual) address of the instruction that jumps.
 *   inst - The instruction that does the jump.
 *   reg - The register used in the jump instruction.
 *   jump_sets - The current information collected about (explicit) jumps
 *     by the jump validator.
 */
static void NaClAddRegisterJumpIndirect64(NaClValidatorState* state,
                                      NaClInstIter* iter,
                                      NaClPcAddress inst_pc,
                                      NaClInstState* inst,
                                      NaClExp* reg,
                                      NaClJumpSets* jump_sets) {
  uint8_t mask;
  NaClInstState* and_state;
  NaClInstState* middle_state;
  NaClInst* and_inst;
  NaClInst* middle_inst;
  int op_1, op_2;
  NaClOpKind and_reg, and_64_reg, jump_reg, middle_reg;
  NaClExpVector* nodes;
  NaClExp* node;
  jump_reg = NaClGetExpRegister(reg);
  DEBUG(NaClLog(LOG_INFO, "checking indirect jump: ");
        NaClInstStateInstPrint(NaClLogGetGio(), inst);
        gprintf(NaClLogGetGio(), "jump_reg = %s\n", NaClOpKindName(jump_reg)));

  /* Do the following block exactly once. Use loop so that "break" can
   * be used for premature exit of block.
   */
  do {
    /* Check and in 3 instruction sequence. */
    if (!NaClInstIterHasLookbackState(iter, 2)) break;
    and_state = NaClInstIterGetLookbackState(iter, 2);
    DEBUG(NaClLog(LOG_INFO, "and?: ");
          NaClInstStateInstPrint(NaClLogGetGio(), and_state));
    and_inst = NaClInstStateInst(and_state);
    if (0x83 != and_inst->opcode[0] || InstAnd != and_inst->name) break;
    DEBUG(NaClLog(LOG_INFO, "and instruction\n"));

    /* Extract the values of the two operands for the and. */
    if (!NaClExtractBinaryOperandIndices(and_state, &op_1, &op_2)) break;
    DEBUG(NaClLog(LOG_INFO, "binary and\n"));

    /* Extract the destination register of the and. */
    nodes = NaClInstStateExpVector(and_state);
    node = &nodes->node[op_1];
    if (ExprRegister != node->kind) break;
    and_reg = NaClGetExpRegister(node);
    DEBUG(NaClLog(LOG_INFO, "and_reg = %s\n", NaClOpKindName(and_reg)));
    and_64_reg = NaClGet64For32BitReg(and_reg);
    DEBUG(NaClLog(LOG_INFO, "and_64_reg = %s\n", NaClOpKindName(and_64_reg)));
    if (RegUnknown == and_64_reg) break;
    DEBUG(NaClLog(LOG_INFO, "registers match!\n"));

    /* Check that the mask is ok. */
    mask = NaClGetJumpMask(state);
    DEBUG(NaClLog(LOG_INFO, "mask = %"NACL_PRIx8"\n", mask));
    assert(0 != mask);  /* alignment must be either 16 or 32. */
    node = &nodes->node[op_2];
    if (ExprConstant != node->kind || mask != node->value) break;
    DEBUG(NaClLog(LOG_INFO, "is mask constant\n"));

    /* Check middle (i.e. lea/add) instruction in 3 instruction sequence. */
    middle_state = NaClInstIterGetLookbackState(iter, 1);
    DEBUG(NaClLog(LOG_INFO, "middle inst: ");
          NaClInstStateInstPrint(NaClLogGetGio(), middle_state));
    middle_inst = NaClInstStateInst(middle_state);

    /* Extract the values of the two operands for the lea/add instruction. */
    if (!NaClExtractBinaryOperandIndices(middle_state, &op_1, &op_2)) break;
    DEBUG(NaClLog(LOG_INFO, "middle is binary, op_1 index = %d\n", op_1));

    /* Extract the destination register of the lea/and, and verify that
     * it is a register.
     */
    nodes = NaClInstStateExpVector(middle_state);
    node = &nodes->node[op_1];
    if (ExprRegister != node->kind) break;

    /* Compare the middle destination register to the jump register. */
    middle_reg = NaClGetExpRegister(node);
    DEBUG(NaClLog(LOG_INFO, "middle reg = %s\n", NaClOpKindName(middle_reg)));
    if (middle_reg != jump_reg) break;

    if (InstLea == middle_inst->name) {
      /* Check that we have [%RBASE + %REG64-A] as second argument to lea */
      node = &nodes->node[op_2];
      if (ExprMemOffset != node->kind ||
          !NaClMemOffsetMatchesBaseIndex(nodes, op_2, state->base_register,
                                     and_64_reg)) {
        break;
      }
    } else if (InstAdd == middle_inst->name) {
      /* Check that the jump register is the 64-bit version of the
       * and register.
       */
      if (jump_reg != and_64_reg) break;

      /* Check that we have %RBASE as second argument to add. */
      if (NaClGetExpVectorRegister(nodes, op_2) != state->base_register) break;
    } else {
      break;
    }

    /* If reached, indirect jump is properly masked. */
    DEBUG(NaClLog(LOG_INFO, "Protect indirect jump instructions\n"));
    NaClMarkInstructionJumpIllegal(state, middle_state);
    NaClMarkInstructionJumpIllegal(state, inst);
    return;
  } while(0);

  /* If reached, mask was not found. */
  NaClValidatorInstMessage(LOG_ERROR, state, inst, "Invalid indirect jump\n");
}

/* Checks if an indirect jump (in 32-bit mode) is native client compliant.
 *
 * Expects pattern:
 *    and %REG, MASK
 *    jmp %REG
 *
 * where the MASK is all 1's except for the alignment mask bits, which must
 * be zero.
 *
 * It is assumed that opcode 0x83 is used for the AND operation, and hence, the
 * mask is a single byte.
 *
 * Note: applies to all kinds of jumps and calls.
 *
 * Parameters:
 *   state - The state of the validator.
 *   iter - The instruction iterator being used.
 *   inst_pc - The (virtual) address of the instruction that jumps.
 *   inst_state - The instruction that does the jump.
 *   reg - The register used in the jump instruction.
 *   jump_sets - The current information collected about (explicit) jumps
 *     by the jump validator.
 */
static void NaClAddRegisterJumpIndirect32(NaClValidatorState* state,
                                          NaClInstIter* iter,
                                          NaClPcAddress inst_pc,
                                          NaClInstState* inst_state,
                                          NaClExp* reg,
                                          NaClJumpSets* jump_sets
                                          ) {
  uint8_t mask;
  NaClInstState* and_state;
  NaClInst* and_inst;
  int op_1, op_2;
  NaClOpKind and_reg, jump_reg;
  NaClExpVector* nodes;
  NaClExp* node;
  assert(ExprRegister == reg->kind);
  jump_reg = NaClGetExpRegister(reg);

  /* Do the following block exactly once. */
  do {
    /* Check that the jump is preceded with an AND. */
    if (!NaClInstIterHasLookbackState(iter, 1)) break;
    and_state = NaClInstIterGetLookbackState(iter, 1);
    and_inst = NaClInstStateInst(and_state);
    if (0x83 != and_inst->opcode[0] || InstAnd  != and_inst->name) break;

    /* Extract the values of the two operands for the and. */
    if (!NaClExtractBinaryOperandIndices(and_state, &op_1, &op_2)) break;


    /* Check that the register of the AND matches the jump. */
    nodes = NaClInstStateExpVector(and_state);
    node = &nodes->node[op_1];
    if (ExprRegister != node->kind) break;
    and_reg = NaClGetExpRegister(node);
    if (jump_reg != and_reg) break;

    /* Check that the mask is ok. */
    mask = NaClGetJumpMask(state);
    assert(0 != mask);  /* alignment must be either 16 or 32. */
    node = &nodes->node[op_2];
    if (ExprConstant != node->kind || mask != node->value) break;

    /* If reached, indirect jump is properly masked. */
    DEBUG(NaClLog(LOG_INFO, "Protect register jump indirect\n"));
    NaClMarkInstructionJumpIllegal(state, inst_state);
    return;
  } while(0);

  /* If reached, mask was not found. */
  NaClValidatorInstMessage(LOG_ERROR, state, inst_state,
                           "Invalid indirect jump\n");
}

/* Given a jump statement, add the corresponding (explicit) jump value
 * to the set of actual jump targets.
 * Parameters:
 *   state - The state of the validator.
 *   iter - The instruction iterator being used.
 *   inst_pc - The (virtual) address of the instruction that jumps.
 *   inst_state - The instruction that does the jump.
 *   jump_sets - The current information collected about (explicit) jumps
 *     by the jump validator.
 */
static void NaClAddExprJumpTarget(NaClValidatorState* state,
                                  NaClInstIter* iter,
                                  NaClPcAddress inst_pc,
                                  NaClJumpSets* jump_sets) {
  uint32_t i;
  NaClInstState* inst_state = state->cur_inst_state;
  NaClExpVector* vector = state->cur_inst_vector;
  DEBUG(NaClLog(LOG_INFO, "jump checking: ");
        NaClInstStateInstPrint(NaClLogGetGio(), inst_state));
  for (i = 0; i < vector->number_expr_nodes; ++i) {
    if (vector->node[i].flags & NACL_EFLAG(ExprJumpTarget)) {
      switch (vector->node[i].kind) {
        case ExprRegister:
          if (64 == NACL_TARGET_SUBARCH) {
            NaClAddRegisterJumpIndirect64(state,
                                          iter,
                                          inst_pc,
                                          inst_state,
                                          &vector->node[i],
                                          jump_sets);
          } else {
            NaClAddRegisterJumpIndirect32(state,
                                          iter,
                                          inst_pc,
                                          inst_state,
                                          &vector->node[i],
                                          jump_sets);
          }
          break;
        case ExprConstant:
        case ExprConstant64: {
          /* Explicit jump value. Allow "call 0" as special case. */
          uint64_t target = NaClGetExpConstant(vector, i);
          if (! (target == 0 &&
                 NaClInstStateInst(inst_state)->name == InstCall)) {
            NaClAddJumpToJumpSets(state,
                                  inst_pc,
                                  (NaClPcAddress) target,
                                  jump_sets,
                                  inst_state);
          }
          break;
        }
        default:
          NaClValidatorInstMessage(
              LOG_ERROR, state, inst_state,
              "Jump not native client compliant\n");
      }
    }
  }
}

/* Given an instruction corresponding to a call, validate that the generated
 * return address is safe.
 * Parameters:
 *   state - The state of the validator.
 *   pc - The (virtual) address of the instruciton that does the call.
 *   inst_state - The instruction that does the call.
 */
static void NaClValidateCallAlignment(NaClValidatorState* state,
                                      NaClPcAddress pc,
                                      NaClInstState* inst_state) {
  /* The return is safe only if it begins at an aligned address (since
   * return instructions are not explicit jumps).
   */
  NaClPcAddress next_pc = pc + NaClInstStateLength(inst_state);
  if (next_pc & state->alignment_mask) {
    DEBUG(NaClLog(LOG_INFO,
                  "Call alignment: pc = %"NACL_PRIxNaClPcAddress", "
                 "next_pc=%"NACL_PRIxNaClPcAddress", "
                 "mask = %"NACL_PRIxNaClPcAddress"\n",
                 pc, next_pc, state->alignment_mask));
    NaClValidatorInstMessage(
        LOG_ERROR, state, inst_state,
        "Bad call alignment, return pc = %"NACL_PRIxNaClPcAddress"\n", next_pc);
  }
}

/* Record that the given address of the given instruction is the beginning of
 * a disassembled instruction.
 * Parameters:
 *   State - The state of the validator.
 *   pc - The (virtual) address of a disassembled instruction.
 *   inst_state - The disassembled instruction.
 *   jump_sets - The current information collected about (explicit) jumps
 *     by the jump validator.
 */
static void NaClRememberIp(NaClValidatorState* state,
                           NaClPcAddress pc,
                           NaClInstState* inst_state,
                           NaClJumpSets* jump_sets) {
  if (pc < state->vbase || pc >= state->vlimit) {
    NaClValidatorInstMessage(LOG_ERROR, state, inst_state,
                             "Instruction pc out of range\n");
  } else {
    DEBUG(NaClLog(LOG_INFO,
                  "Add possible jump address: %"NACL_PRIxNaClPcAddress"\n",
                  pc));
    NaClAddressSetAdd(jump_sets->possible_targets, pc, state);
  }
}

void NaClJumpValidator(NaClValidatorState* state,
                       NaClInstIter* iter,
                       NaClJumpSets* jump_sets) {
  NaClInstState* inst_state = state->cur_inst_state;
  NaClPcAddress pc = NaClInstStateVpc(inst_state);
  NaClRememberIp(state, pc, inst_state, jump_sets);
  switch (state->cur_inst->name) {
    case InstJb:
    case InstJbe:
    case InstJcxz:
    case InstJecxz:
    case InstJrcxz:
    case InstJnl:
    case InstJnle:
    case InstJl:
    case InstJle:
    case InstJmp:
    case InstJnb:
    case InstJnbe:
    case InstJno:
    case InstJnp:
    case InstJns:
    case InstJnz:
    case InstJo:
    case InstJp:
    case InstJs:
    case InstJz:
    case InstLoop:
    case InstLoope:
    case InstLoopne:
      NaClAddExprJumpTarget(state, iter, pc, jump_sets);
      break;
    case InstCall:
      NaClAddExprJumpTarget(state, iter, pc, jump_sets);
      NaClValidateCallAlignment(state, pc, inst_state);
      break;
    default:
      break;
  }
}

/* Returns true if the given address corresponds to the beginning
 * of an atomic sequence of instructions, and hence can be branched to.
 */
static Bool IsNaClReachableAddress(NaClValidatorState* state,
                                   NaClPcAddress addr,
                                   NaClJumpSets* jump_sets) {
  DEBUG(NaClLog(LOG_INFO, "possible contains: %d\n",
                (int) NaClAddressSetContains(jump_sets->possible_targets,
                                             addr, state)));
  DEBUG(NaClLog(LOG_INFO, "removed contains: %d\n",
                (int) NaClAddressSetContains(jump_sets->removed_targets,
                                             addr, state)));
  return NaClAddressSetContains(jump_sets->possible_targets, addr, state) &&
      !NaClAddressSetContains(jump_sets->removed_targets, addr, state);
}

void NaClJumpValidatorSummarize(NaClValidatorState* state,
                                NaClInstIter* iter,
                                NaClJumpSets* jump_sets) {
  /* Check that any explicit jump is to a possible (atomic) sequence
   * of disassembled instructions.
   */
  NaClPcAddress addr;
  size_t i;
  if (state->quit) return;
  NaClValidatorMessage(
      LOG_INFO, state,
      "Checking jump targets: %"NACL_PRIxNaClPcAddress
      " to %"NACL_PRIxNaClPcAddress"\n",
      state->vbase, state->vlimit);

  /* Note: The following is the high-level code that has been commented
   * out, because it is too slow. It represents slightly more than
   * 15% of the overall validator time.
   *
  for (addr = state->vbase; addr < state->vlimit; addr++) {
    if (NaClAddressSetContains(jump_sets->actual_targets, addr, state)) {
      DEBUG(NaClLog(LOG_INFO,
                    "Checking jump address: %"NACL_PRIxNaClPcAddress"\n",
                    addr));
      if (!IsNaClReachableAddress(state, addr, jump_sets)) {
        NaClValidatorPcAddressMessage(LOG_ERROR, state, addr,
                                      "Bad jump target\n");
      }
    }
  }
  */

  /* (Low level) Walk the collected sets to find address that correspond
   * to branches into an atomic sequence of instructions.
   */
  for (i = 0; i < jump_sets->set_array_size; ++i) {
    uint8_t problem = jump_sets->actual_targets[i] &
        (~jump_sets->possible_targets[i] |
         jump_sets->removed_targets[i]);
    if (problem) {
      /* Some bit in this range is a problem, so we will convert back
       * to code like the above and test each bit separately.
       */
      NaClPcAddress j;
      NaClPcAddress base = (i << 3) + state->vbase;
      for (j = 0; j < 8; ++j) {
        addr = base + j;
        if (addr < state->vlimit) {
          if (NaClAddressSetContains(jump_sets->actual_targets, addr, state)) {
            DEBUG(NaClLog(LOG_INFO,
                          "Checking jump address: %"NACL_PRIxNaClPcAddress"\n",
                          addr));
            if (!IsNaClReachableAddress(state, addr, jump_sets)) {
              NaClValidatorPcAddressMessage(LOG_ERROR, state, addr,
                                            "Bad jump target\n");
            }
          }
        }
      }
    }
  }

  /* Check that all block boundaries are accessable at an aligned address. */
  NaClValidatorMessage(
      LOG_INFO, state, "Checking that basic blocks are aligned\n");
  if (state->vbase & state->alignment_mask) {
    NaClValidatorMessage(LOG_ERROR, state,
                         "Code segment starts at 0x%"NACL_PRIxNaClPcAddress", "
                         "which isn't aligned properly.\n",
                         state->vbase);
  } else {
    for (addr = state->vbase; addr < state->vlimit; addr += state->alignment) {
      DEBUG(NaClLog(LOG_INFO,
                    "Checking block address: %"NACL_PRIxNaClPcAddress"\n",
                   addr));
      if (!IsNaClReachableAddress(state, addr, jump_sets)) {
        NaClValidatorPcAddressMessage(
            LOG_ERROR, state, addr, "Bad basic block alignment.\n");
      }
    }
  }
}

void NaClJumpValidatorDestroy(NaClValidatorState* state,
                              NaClJumpSets* jump_sets) {
  NaClAddressSetDestroy(jump_sets->actual_targets);
  NaClAddressSetDestroy(jump_sets->possible_targets);
  NaClAddressSetDestroy(jump_sets->removed_targets);
  free(jump_sets);
}

void NaClMarkInstructionJumpIllegal(struct NaClValidatorState* state,
                                    struct NaClInstState* inst) {
  NaClPcAddress pc = NaClInstStateVpc(inst);
  if (pc < state->vbase || pc >= state->vlimit) {
    /* ERROR instruction out of range.
     * Note: Not reported here, because this will already be reported by
     * the call to NaClRememberIp in JumpValidator.
     */
  } else {
    NaClJumpSets* jump_sets =
        (NaClJumpSets*)
        NaClGetValidatorLocalMemory((NaClValidator) NaClJumpValidator,
                                    state);
    DEBUG(NaClLog(LOG_INFO,
                  "Mark instruction as jump illegal: %"NACL_PRIxNaClPcAddress
                 "\n",
                 pc));
    NaClAddressSetAdd(jump_sets->removed_targets, pc, state);
  }
}
