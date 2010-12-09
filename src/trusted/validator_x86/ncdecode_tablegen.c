/* Copyright (c) 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * New table driven generator for a decoder of x86 code.
 *
 * Note: Most of the organization of this document is based on the
 * NaClOpcode Map appendix of one of the following documents:

 * (1) "Intel 64 and IA-32 Architectures
 * Software Developer's Manual (volumes 1, 2a, and 2b; documents
 * 253665.pdf, 253666.pdf, and 253667.pdf)".
 *
 * (2) "Intel 80386 Reference Programmer's Manual" (document
 * http://pdos.csail.mit.edu/6.828/2004/readings/i386/toc.htm).
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <string.h>

#define NEEDSNACLINSTTYPESTRING
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"


#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/utils/flags.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_st.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* Define the default buffer size to use. */
#define BUFFER_SIZE 256

/* Define the number of possible bytes values. */
#define NACL_NUM_BYTE_VALUES 256

/* Model an NaClInst, sorted by OpcodeInModRm values. */
typedef struct NaClMrmInst {
  NaClInst inst;
  struct NaClMrmInst* next;
} NaClMrmInst;

/* Note: in general all errors in this module will be fatal.
 * To debug: use gdb or your favorite debugger.
 */
void NaClFatal(const char* s) {
  NaClLog(LOG_FATAL, "Error: %s\n", s);
  NaClLog(LOG_FATAL, "Fatal: cannot recover\n");
  exit(-1);
}

/* Returns the print name of the given run mode. */
static const char* NaClRunModeName(NaClRunMode mode) {
  switch (mode) {
    case X86_32: return "x86-32 bit mode";
    case X86_64: return "x86-64 bit mode";
    default: assert(0);
  }

  /* NOTREACHED */
  return NULL;
}

/* Defines the run mode files that should be generated. */
NaClRunMode NACL_FLAGS_run_mode = NaClRunModeSize;

/* Defines whether the output of the instruction set should
 * be as a header file, or human readable (for documentation).
 */
static Bool NACL_FLAGS_human_readable = FALSE;

/* Holds the current instruction prefix. */
static NaClInstPrefix current_opcode_prefix = NoPrefix;

/* Holds the default instruction prefix. */
static NaClInstPrefix default_opcode_prefix = NoPrefix;

/* Holds the current instruction being built. */
static NaClInst* current_inst = NULL;

/* Holds the root of the trie of known instruction opcode sequences. */
static NaClInstNode* inst_node_root = NULL;

/* Holds the current opcode sequence to be associated with the next
 * defined opcode.
 */
static NaClInstNode* current_inst_node = NULL;

/* Holds the candidate for the current_inst_node, when
 * NaClDefInst is called.
 */
static NaClInstNode* current_cand_inst_node = NULL;

/* Holds the current instruction with mrm extention being built. */
static NaClMrmInst* current_inst_mrm = NULL;

/* Holds the instruction to model instructions that can't be parsed. */
static NaClInst* undefined_inst = NULL;

/* True if we are to apply sanity checks as we define operantions. */
static Bool apply_sanity_checks = TRUE;

/* Holds all defined opcodes. */
static NaClInst* NaClInstTable[NCDTABLESIZE][NaClInstPrefixEnumSize];

#define NACL_DEFAULT_CHOICE_COUNT (-1)

#define NACL_NO_MODRM_OPCODE 8

#define NACL_MODRM_OPCODE_SIZE (NACL_NO_MODRM_OPCODE + 1)

/* Holds the expected number of entries in the defined instructions.
 * Note: the last index corresponds to the modrm opcode, or
 * NACL_NO_MODRM_OPCODE if no modrm opcode.
 */
static int NaClInstCount[NCDTABLESIZE]
          [NaClInstPrefixEnumSize][NACL_MODRM_OPCODE_SIZE];

static NaClMrmInst* NaClInstMrmTable[NCDTABLESIZE]
          [NaClInstPrefixEnumSize][NACL_MODRM_OPCODE_SIZE];

/* Holds encodings of prefix bytes. */
static const char* NaClPrefixTable[NCDTABLESIZE];

/* Prints out the opcode prefix being defined, the opcode pattern
 * being defined, and the given error message. Then aborts the
 * execution of the program.
 */
static void NaClFatalInst(const char* message) {
  NaClLog(LOG_INFO, "Prefix: %s\n", NaClInstPrefixName(current_opcode_prefix));
  NaClInstPrint(NaClLogGetGio(), current_inst);
  NaClFatal(message);
}

/* Prints out what operand is currently being defined, followed by the given
 * error message. Then aborts the execution of the program.
 */
static void NaClFatalOp(int index, const char* message) {
  if (0 <= index && index <= current_inst->num_operands) {
    NaClLog(LOG_ERROR, "On operand %d: %s\n", index,
            NaClOpKindName(current_inst->operands[index].kind));
  } else {
    NaClLog(LOG_ERROR, "On operand %d:\n", index);
  }
  NaClFatalInst(message);
}

/* Advance the buffer/buffer_size values by count characters. */
static void CharAdvance(char** buffer, size_t* buffer_size, size_t count) {
  if (count < 0) {
    NaClFatal("Unable to advance buffer by count!");
  } else {
    *buffer += count;
    *buffer_size += count;
  }
}

/* Generates a (malloc allocated) string describing the form for the
 * operands.
 * Note: This code used to be part of the validator (runtime) library.
 * By moving it here, we remove this code, and runtime cost, from
 * the validator runtime.
 */
static char* NaClDefaultOperandsDesc(NaClInst* inst) {
  char buffer[BUFFER_SIZE];
  char* buf = buffer;
  size_t buf_size = BUFFER_SIZE;
  Bool is_first = TRUE;
  int i;
  buffer[0] = '\0';  /* just in case there isn't any operands. */
  for (i = 0; i < inst->num_operands; ++i) {
    Bool is_implicit = (NACL_EMPTY_OPFLAGS != (inst->operands[i].flags &
                                               NACL_OPFLAG(OpImplicit)));
    NaClOpKind kind = inst->operands[i].kind;
    if (is_first) {
      is_first = FALSE;
    } else {
      CharAdvance(&buf, &buf_size,
                  SNPRINTF(buf, buf_size, ", "));
    }
    if (is_implicit) {
      CharAdvance(&buf, &buf_size,
                  SNPRINTF(buf, buf_size, "{"));
    }
    switch (kind) {
      case A_Operand:
      case Aw_Operand:
      case Av_Operand:
      case Ao_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$A"));
        break;
      case E_Operand:
      case Eb_Operand:
      case Ew_Operand:
      case Ev_Operand:
      case Eo_Operand:
      case Edq_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$E"));
        break;
      case G_Operand:
      case Gb_Operand:
      case Gw_Operand:
      case Gv_Operand:
      case Go_Operand:
      case Gdq_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$G"));
        break;
      case Seg_G_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$SegG"));
        break;
      case G_OpcodeBase:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$/r"));
        break;
      case I_Operand:
      case Ib_Operand:
      case Iw_Operand:
      case Iv_Operand:
      case Io_Operand:
      case I2_Operand:
      case J_Operand:
      case Jb_Operand:
      case Jw_Operand:
      case Jv_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$I"));
        break;
      case M_Operand:
      case Mb_Operand:
      case Mw_Operand:
      case Mv_Operand:
      case Mo_Operand:
      case Mdq_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$M"));
        break;
      case Mpw_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "m16:16"));
        break;
      case Mpv_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "m16:32"));
        break;
      case Mpo_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "m16:64"));
        break;
      case Mmx_E_Operand:
      case Mmx_N_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$E(mmx)"));
        break;
      case Mmx_G_Operand:
      case Mmx_Gd_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$G(mmx)"));
        break;
      case Xmm_E_Operand:
      case Xmm_Eo_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$E(xmm)"));
        break;
      case Xmm_G_Operand:
      case Xmm_Go_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$G(xmm)"));
        break;
      case O_Operand:
      case Ob_Operand:
      case Ow_Operand:
      case Ov_Operand:
      case Oo_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "$O"));
        break;
      case S_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "%%Sreg"));
        break;
      case C_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "%%Creg"));
        break;
      case D_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "%%Dreg"));
        break;
      case St_Operand:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "%%st"));
        break;
      case RegAL:
      case RegBL:
      case RegCL:
      case RegDL:
      case RegAH:
      case RegBH:
      case RegCH:
      case RegDH:
      case RegDIL:
      case RegSIL:
      case RegBPL:
      case RegSPL:
      case RegR8B:
      case RegR9B:
      case RegR10B:
      case RegR11B:
      case RegR12B:
      case RegR13B:
      case RegR14B:
      case RegR15B:
      case RegAX:
      case RegBX:
      case RegCX:
      case RegDX:
      case RegSI:
      case RegDI:
      case RegBP:
      case RegSP:
      case RegR8W:
      case RegR9W:
      case RegR10W:
      case RegR11W:
      case RegR12W:
      case RegR13W:
      case RegR14W:
      case RegR15W:
      case RegEAX:
      case RegEBX:
      case RegECX:
      case RegEDX:
      case RegESI:
      case RegEDI:
      case RegEBP:
      case RegESP:
      case RegR8D:
      case RegR9D:
      case RegR10D:
      case RegR11D:
      case RegR12D:
      case RegR13D:
      case RegR14D:
      case RegR15D:
      case RegCS:
      case RegDS:
      case RegSS:
      case RegES:
      case RegFS:
      case RegGS:
      case RegEFLAGS:
      case RegRFLAGS:
      case RegEIP:
      case RegRIP:
      case RegRAX:
      case RegRBX:
      case RegRCX:
      case RegRDX:
      case RegRSI:
      case RegRDI:
      case RegRBP:
      case RegRSP:
      case RegR8:
      case RegR9:
      case RegR10:
      case RegR11:
      case RegR12:
      case RegR13:
      case RegR14:
      case RegR15:
      case RegREIP:
      case RegREAX:
      case RegREBX:
      case RegRECX:
      case RegREDX:
      case RegRESP:
      case RegREBP:
      case RegRESI:
      case RegREDI:
      case RegDS_EDI:
      case RegDS_EBX:
      case RegES_EDI:
      case RegST0:
      case RegST1:
      case RegST2:
      case RegST3:
      case RegST4:
      case RegST5:
      case RegST6:
      case RegST7:
      case RegMMX0:
      case RegMMX1:
      case RegMMX2:
      case RegMMX3:
      case RegMMX4:
      case RegMMX5:
      case RegMMX6:
      case RegMMX7:
      case RegXMM0:
      case RegXMM1:
      case RegXMM2:
      case RegXMM3:
      case RegXMM4:
      case RegXMM5:
      case RegXMM6:
      case RegXMM7:
      case RegXMM8:
      case RegXMM9:
      case RegXMM10:
      case RegXMM11:
      case RegXMM12:
      case RegXMM13:
      case RegXMM14:
      case RegXMM15:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "%%%s",
                             NaClOpKindName(kind) + strlen("Reg")));
        break;
      case Const_1:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "1"));
        break;
      default:
        CharAdvance(&buf, &buf_size,
                    SNPRINTF(buf, buf_size, "???"));
        break;
    }
    if (is_implicit) {
      CharAdvance(&buf, &buf_size,
                  SNPRINTF(buf, buf_size, "}"));
    }
  }
  return strdup(buffer);
}

/* Define the prefix name for the given opcode, for the given run mode. */
static void NaClEncodeModedPrefixName(const uint8_t byte, const char* name,
                                      const NaClRunMode mode) {
  if (NACL_FLAGS_run_mode == mode) {
    NaClPrefixTable[byte] = name;
  }
}

/* Define the prefix name for the given opcode, for all run modes. */
static void NaClEncodePrefixName(const uint8_t byte, const char* name) {
  NaClEncodeModedPrefixName(byte, name, NACL_FLAGS_run_mode);
}

/* Change the current opcode prefix to the given value. */
void NaClDefInstPrefix(const NaClInstPrefix prefix) {
  current_opcode_prefix = prefix;
}

void NaClResetToDefaultInstPrefix() {
  NaClDefInstPrefix(default_opcode_prefix);
}

NaClInst* NaClGetDefInst() {
  return current_inst;
}

/* Check if an E_Operand operand has been repeated, since it should
 * never appear for more than one argument. If repeated, generate an
 * appropriate error message and terminate.
 */
static void NaClCheckIfERepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    NaClOp* operand = &current_inst->operands[i];
    switch (operand->kind) {
      case Mmx_E_Operand:
      case Xmm_E_Operand:
      case Xmm_Eo_Operand:
      case E_Operand:
      case Eb_Operand:
      case Ew_Operand:
      case Ev_Operand:
      case Eo_Operand:
      case Edq_Operand:
        NaClFatalOp(index, "Can't use E Operand more than once");
        break;
      default:
        break;
    }
  }
}

/* Called if operand is a G_Operand. Checks that the opcode doesn't specify
 * that the REG field of modrm is an opcode, since G_Operand doesn't make
 * sense in such cases.
 */
static void NaClCheckIfOpcodeInModRm(int index) {
  if ((current_inst->flags & NACL_IFLAG(OpcodeInModRm)) &&
      (NACL_EMPTY_OPFLAGS == (current_inst->operands[index].flags &
                              NACL_IFLAG(AllowGOperandWithOpcodeInModRm)))) {
    NaClFatalOp(index,
                 "Can't use G Operand, bits being used for opcode in modrm");
  }
}

/* Check if an G_Operand operand has been repeated, since it should
 * never appear for more than one argument. If repeated, generate an
 * appropriate error message and terminate.
 */
static void NaClCheckIfGRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    NaClOp* operand = &current_inst->operands[i];
    switch (operand->kind) {
      case Mmx_G_Operand:
      case Xmm_G_Operand:
      case Xmm_Go_Operand:
      case G_Operand:
      case Gb_Operand:
      case Gw_Operand:
      case Gv_Operand:
      case Go_Operand:
      case Gdq_Operand:
        NaClFatalOp(index, "Can't use G Operand more than once");
        break;
      default:
        break;
    }
  }
}

/* Check if an I_Operand/J_OPerand operand has been repeated, since it should
 * never appear for more than one argument (both come from the immediate field
 * of the instruction). If repeated, generate an appropriate error message
 * and terminate.
 */
static void NaClCheckIfIRepeated(int index) {
  int i;
  for (i = 0; i < index; ++i) {
    NaClOp* operand = &current_inst->operands[i];
    switch (operand->kind) {
      case I_Operand:
      case Ib_Operand:
      case Iw_Operand:
      case Iv_Operand:
      case Io_Operand:
        NaClFatalOp(index, "Can't use I_Operand more than once");
        break;
      case J_Operand:
      case Jb_Operand:
      case Jv_Operand:
        NaClFatalOp(index, "Can't use both I_Operand and J_Operand");
        break;
      default:
        break;
    }
  }
}

/* Returns the set of operand size flags defined for the given instruction. */
NaClIFlags NaClOperandSizes(NaClInst* inst) {
  NaClIFlags flags = inst->flags & (NACL_IFLAG(OperandSize_b) |
                                    NACL_IFLAG(OperandSize_w) |
                                    NACL_IFLAG(OperandSize_v) |
                                    NACL_IFLAG(OperandSize_o));
  /* Note: if no sizes specified, assume all sizes possible. */
  if (NACL_EMPTY_IFLAGS == flags) {
    flags = NACL_IFLAG(OperandSize_b) |
        NACL_IFLAG(OperandSize_w) |
        NACL_IFLAG(OperandSize_v) |
        NACL_IFLAG(OperandSize_o);
  }
  return flags;
}

/* Check that the operand being defined (via the given index), does not
 * specify any inconsistent flags.
 */
static void NaClApplySanityChecksToOp(int index) {
  NaClOp* operand = &current_inst->operands[index];
  const NaClIFlags operand_sizes = NaClOperandSizes(current_inst);

  if (!apply_sanity_checks) return;

  /* Check that operand is consistent with other operands defined, or flags
   * defined on the opcode.
   */
  switch (operand->kind) {
    case E_Operand:
      NaClCheckIfERepeated(index);
      break;
    case Eb_Operand:
      if (NACL_IFLAG(OperandSize_b) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_b, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Ew_Operand:
      if (NACL_IFLAG(OperandSize_w) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_w, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Ev_Operand:
      if (NACL_IFLAG(OperandSize_v) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_v, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Eo_Operand:
      if (NACL_IFLAG(OperandSize_o) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_o, use E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case Edq_Operand:
      NaClCheckIfERepeated(index);
      break;
    case Mmx_E_Operand:
    case Xmm_E_Operand:
      NaClCheckIfERepeated(index);
      break;
    case Xmm_Eo_Operand:
      if (NACL_IFLAG(OperandSize_o) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp
            (index,
             "Size implied by OperandSize_o, use Xmm_E_Operand instead");
      }
      NaClCheckIfERepeated(index);
      break;
    case G_Operand:
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gb_Operand:
      if (NACL_IFLAG(OperandSize_b) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_b, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gw_Operand:
      if (NACL_IFLAG(OperandSize_w) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_w, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gv_Operand:
      if (NACL_IFLAG(OperandSize_v) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_v, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Go_Operand:
      if (NACL_IFLAG(OperandSize_o) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_o, use G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Gdq_Operand:
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Mmx_G_Operand:
    case Xmm_G_Operand:
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case Xmm_Go_Operand:
      if (NACL_IFLAG(OperandSize_o) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp
            (index,
             "Size implied by OperandSize_o, use Xmm_G_Operand instead");
      }
      NaClCheckIfGRepeated(index);
      NaClCheckIfOpcodeInModRm(index);
      break;
    case I_Operand:
      NaClCheckIfIRepeated(index);
      break;
    case Ib_Operand:
      if (NACL_IFLAG(OperandSize_b) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_b, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_b)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_b, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case Iw_Operand:
      if (NACL_IFLAG(OperandSize_w) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_w, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_w)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_w, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case Iv_Operand:
      if (NACL_IFLAG(OperandSize_v) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_v, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_v)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_v, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    case Io_Operand:
      if (NACL_IFLAG(OperandSize_o) == operand_sizes) {
        /* Singlton set already specifies size, so no need for extra
         * size specification.
         */
        NaClFatalOp(index,
                     "Size implied by OperandSize_o, use I_Operand instead");
      }
      if (current_inst->flags & NACL_IFLAG(OpcodeHasImmed_o)) {
        NaClFatalOp(index,
                     "Size implied by OpcodeHasImmed_o, use I_Operand instead");
      }
      NaClCheckIfIRepeated(index);
      break;
    default:
      break;
  }
}

/* Define the next operand of the current opcode to have
 * the given kind and flags.
 */
static void NaClDefOpInternal(NaClOpKind kind, NaClOpFlags flags) {
  int index;
  assert(NULL != current_inst);
  index = current_inst->num_operands++;
  current_inst->operands[index].kind = kind;
  current_inst->operands[index].flags = flags;
}

static void NaClInstallCurrentIntoOpcodeMrm(const NaClInstPrefix prefix,
                                            const uint8_t opcode,
                                            int mrm_index) {
  DEBUG(NaClLog(LOG_INFO, "  Installing into [%x][%s][%d]\n",
                opcode, NaClInstPrefixName(prefix), mrm_index));
  if (NULL == NaClInstMrmTable[opcode][prefix][mrm_index]) {
    NaClInstMrmTable[opcode][prefix][mrm_index] = current_inst_mrm;
  } else {
    NaClMrmInst* next = NaClInstMrmTable[opcode][prefix][mrm_index];
    while (NULL != next->next) {
      next = next->next;
    }
    next->next = current_inst_mrm;
  }
}

/* Removes the current_inst_mrm from the corresponding
 * NaClInstTable.
 *
 * Used when Opcode32Only or Opcode64Only flag is added, and
 * the flag doesn't match the subarchitecture being modeled.
 */
static void NaClRemoveCurrentInstMrmFromInstTable() {
  uint8_t opcode = current_inst->opcode[current_inst->num_opcode_bytes - 1];
  NaClInst* prev = NULL;
  NaClInst* next = NaClInstTable[opcode][current_inst->prefix];
  while (NULL != next) {
    if (current_inst == next) {
      /* Found - remove! */
      if (NULL == prev) {
        NaClInstTable[opcode][current_inst->prefix] = next->next_rule;
      } else {
        prev->next_rule = next->next_rule;
      }
      return;
    } else  {
      prev = next;
      next = next->next_rule;
    }
  }
}

/* Removes the current_inst_mrm from the corresponding
 * NaClInstMrmTable.
 *
 * Used when Opcode32Only or Opcode64Only flag is added, and
 * the flag doesn't match the subarchitecture being modeled.
 */
static void NaClRemoveCurrentInstMrmFromInstMrmTable() {
  /* Be sure to try opcode in first operand (if applicable),
   * and the default list NACL_NO_MODRM_OPCODE, in case
   * the operand hasn't been processed yet.
   */
  int mrm_opcode = NACL_NO_MODRM_OPCODE;
  if (current_inst->flags & NACL_IFLAG(OpcodeInModRm)) {
    mrm_opcode = current_inst->opcode[current_inst->num_opcode_bytes];
  }

  while (1) {
    uint8_t opcode = current_inst->opcode[current_inst->num_opcode_bytes - 1];
    NaClMrmInst* prev = NULL;
    NaClMrmInst* next =
        NaClInstMrmTable[opcode][current_inst->prefix][mrm_opcode];
    DEBUG(NaClLog(LOG_INFO, "Removing [%02x][%s][%d]?",
                  opcode, NaClInstPrefixName(current_inst->prefix),
                  mrm_opcode));
    while (NULL != next) {
      if (current_inst_mrm == next) {
        /* Found - remove! */
        if (NULL == prev) {
          NaClInstMrmTable[opcode][current_inst->prefix][mrm_opcode] =
              next->next;
        } else {
          prev->next = next->next;
        }
        return;
      } else {
        prev = next;
        next = next->next;
      }
    }
    if (mrm_opcode == NACL_NO_MODRM_OPCODE) return;
    mrm_opcode = NACL_NO_MODRM_OPCODE;
  }
}

static void NaClMoveCurrentToMrmIndex(int mrm_index) {
  /* First remove from default location. */
  uint8_t opcode = current_inst->opcode[current_inst->num_opcode_bytes - 1];
  NaClMrmInst* prev = NULL;
  NaClMrmInst* next =
      NaClInstMrmTable[opcode][current_opcode_prefix]
                      [NACL_NO_MODRM_OPCODE];
  while (current_inst_mrm != next) {
    if (next == NULL) return;
    prev = next;
    next = next->next;
  }
  if (NULL == prev) {
    NaClInstMrmTable[opcode][current_opcode_prefix]
                    [NACL_NO_MODRM_OPCODE] =
        next->next;
  } else {
    prev->next = next->next;
  }
  current_inst_mrm = next;
  current_inst_mrm->next = NULL;
  NaClInstallCurrentIntoOpcodeMrm(current_opcode_prefix, opcode, mrm_index);
}

static void NaClPrintlnOpFlags(struct Gio* g, NaClOpFlags flags) {
  int i;
  for (i = 0; i < NaClOpFlagEnumSize; ++i) {
    if (flags & NACL_OPFLAG(i)) {
      gprintf(g, " %s", NaClOpFlagName(i));
    }
  }
  gprintf(g, "\n");
}

static void NaClApplySanityChecksToInst();

void NaClDefOpcodeExtension(int opcode) {
  uint8_t byte_opcode;
  byte_opcode = (uint8_t) opcode;
  if ((opcode < 0) || (opcode > 7)) {
    NaClFatalInst("Attempted to define opcode extension not in range [0..7]");
  }
  if (NACL_EMPTY_IFLAGS ==
      (current_inst->flags & NACL_IFLAG(OpcodeInModRm))) {
    NaClFatalInst(
        "Opcode extension in [0..7], but not OpcodeInModRm");
  }
  DEBUG(NaClLog(LOG_INFO, "Defining opcode extension %d\n", opcode));
  NaClMoveCurrentToMrmIndex(byte_opcode);
  current_inst->opcode[current_inst->num_opcode_bytes] = byte_opcode;
}

void NaClDefineOpcodeModRmRmExtension(int value) {
  uint8_t byte_opcode = (uint8_t) value;
  if ((value < 0) || (value > 7)) {
    NaClFatalInst("Attempted to defined Opcode modrm rm extension "
                  "not in range [0..7]");
  }
  if (NACL_EMPTY_IFLAGS ==
      (current_inst->flags & NACL_IFLAG(OpcodeInModRm))) {
    NaClFatalInst(
        "Opcode modrm rm extension in [0..7], but not OpcodeInModRm");
  }
  DEBUG(NaClLog(LOG_INFO, "Defining modrm r/m opcode extension %d", value));
  NaClAddIFlags(NACL_IFLAG(OpcodeInModRmRm));
  if (current_inst->num_opcode_bytes + 1 < NACL_MAX_ALL_OPCODE_BYTES) {
    current_inst->opcode[current_inst->num_opcode_bytes+1] = byte_opcode;
  } else {
    NaClFatalInst("No room for opcode modrm rm extension");
  }
}

void NaClDefOpcodeRegisterValue(int r) {
  uint8_t byte_r;
  byte_r = (uint8_t) r;
  if ((r < 0) || (r > 7)) {
    NaClFatalInst("Attempted to define an embedded opcode register value "
                  "not in range [0.. 7]");
  }
  if (NACL_EMPTY_IFLAGS ==
      (current_inst->flags & NACL_IFLAG(OpcodePlusR))) {
    NaClFatalInst(
        "Attempted to define opcode register value when not OpcodePlusR");
  }
  current_inst->opcode[current_inst->num_opcode_bytes] = byte_r;
}

/* Same as previous function, except that sanity checks
 * are applied to see if inconsistent information is
 * being defined.
 */
void NaClDefOp(
    NaClOpKind kind,
    NaClOpFlags flags) {
  int index = current_inst->num_operands;
  DEBUG(NaClLog(LOG_INFO, "  %s:", NaClOpKindName(kind));
        NaClPrintlnOpFlags(NaClLogGetGio(), flags));
  if (NACL_MAX_NUM_OPERANDS <= index) {
    NaClFatalOp(index, "NaClOpcode defines too many operands...\n");
  }
  /* If one of the M_Operands, make sure that the ModRm mod field isn't 0x3,
   * so that we don't return registers.
   * If one specifies an operand that implies the use of a ModRm byte, add
   * the corresponding flag.
   * Note: See section A.2.5 of Intel manual (above) for an explanation of the
   * ModRm mod field being any value except 0x3, for values having an
   * explicit memory operand.
   */
  switch (kind) {
    case M_Operand:
    case Mb_Operand:
    case Mw_Operand:
    case Mv_Operand:
    case Mo_Operand:
    case Mdq_Operand:
    case Mpw_Operand:
    case Mpv_Operand:
    case Mpo_Operand:
      NaClAddIFlags(NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(ModRmModIsnt0x3));
      break;
    case Mmx_N_Operand:
      kind = Mmx_E_Operand;
      NaClAddIFlags(NACL_IFLAG(OpcodeUsesModRm) | NACL_IFLAG(ModRmModIs0x3));
      /* Automatically fall to the next case. */
    case E_Operand:
    case Eb_Operand:
    case Ew_Operand:
    case Ev_Operand:
    case Eo_Operand:
    case Edq_Operand:
    case G_Operand:
    case Gb_Operand:
    case Gw_Operand:
    case Gv_Operand:
    case Go_Operand:
    case Gdq_Operand:
    case Mmx_G_Operand:
    case Mmx_Gd_Operand:
    case Xmm_E_Operand:
    case Xmm_Eo_Operand:
    case Xmm_G_Operand:
    case Xmm_Go_Operand:
      NaClAddIFlags(NACL_IFLAG(OpcodeUsesModRm));
      break;
    default:
      break;
  }
  /* Define and apply sanity checks. */
  NaClDefOpInternal(kind, flags);
  NaClApplySanityChecksToOp(index);
}

void NaClAddOpFlags(uint8_t operand_index, NaClOpFlags more_flags) {
  DEBUG(
      struct Gio* g = NaClLogGetGio();
      gprintf(g, "Adding flags:");
      NaClPrintlnOpFlags(g, more_flags));
  if (operand_index <= current_inst->num_operands) {
    current_inst->operands[operand_index].flags |= more_flags;
    NaClApplySanityChecksToOp(operand_index);
  } else {
    NaClFatalOp((int) operand_index, "NaClAddOpFlags: index out of range\n");
  }
}

void NaClAddOperandFlags(uint8_t operand_index, NaClOpFlags more_flags) {
  NaClAddOpFlags(operand_index, more_flags);
}

void NaClRemoveOpFlags(uint8_t operand_index, NaClOpFlags more_flags) {
  DEBUG(NaClLog(LOG_INFO, "Removing flags:");
        NaClPrintlnOpFlags(NaClLogGetGio(), more_flags));
  if (operand_index <= current_inst->num_operands) {
    current_inst->operands[operand_index].flags &= ~more_flags;
    NaClApplySanityChecksToOp(operand_index);
  } else {
    NaClFatalOp((int) operand_index, "NaClRemoveOpFlags: index out of range\n");
  }
}

/* Returns true if the given opcode flags are consistent
 * with the value of NACL_FLAGS_run_mode.
 */
static Bool NaClIFlagsMatchesRunMode(NaClIFlags flags) {
  if (flags & NACL_IFLAG(Opcode32Only)) {
    if (flags & NACL_IFLAG(Opcode64Only)) {
      NaClFatal("Can't specify both Opcode32Only and Opcode64Only");
    }
    return NACL_FLAGS_run_mode == X86_32;
  } else if (flags & NACL_IFLAG(Opcode64Only)) {
    return NACL_FLAGS_run_mode == X86_64;
  } else if (flags & NACL_IFLAG(Opcode32Only)) {
    return NACL_FLAGS_run_mode == X86_32;
  } else {
    return TRUE;
  }
}

/* Check that the flags defined for an opcode make sense. */
static void NaClApplySanityChecksToInst() {
  const NaClIFlags operand_sizes = NaClOperandSizes(current_inst);
  if (!apply_sanity_checks) return;
  if ((current_inst->flags & NACL_IFLAG(Opcode32Only)) &&
      (current_inst->flags & NACL_IFLAG(Opcode64Only))) {
    NaClFatalInst("Can't be both Opcode32Only and Opcode64Only");
  }
  /* Fix case where both OperandSize_w and SizeIgnoresData16 are specified. */
  if ((current_inst->flags & NACL_IFLAG(OperandSize_w)) &&
      (current_inst->flags & NACL_IFLAG(SizeIgnoresData16))) {
    current_inst->flags &= ~NACL_IFLAG(OperandSize_w);
  }
  if ((current_inst->flags & NACL_IFLAG(OperandSize_b)) &&
      (current_inst->flags & (NACL_IFLAG(OperandSize_w) |
                              NACL_IFLAG(OperandSize_v) |
                              NACL_IFLAG(OperandSize_o) |
                              NACL_IFLAG(OperandSizeDefaultIs64) |
                              NACL_IFLAG(OperandSizeForce64)))) {
    NaClFatalInst(
        "Can't specify other operand sizes when specifying OperandSize_b");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeInModRm)) &&
      (current_inst->flags & NACL_IFLAG(OpcodePlusR))) {
    NaClFatalInst(
        "Can't specify both OpcodeInModRm and OpcodePlusR");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeHasImmed_b)) &&
      (operand_sizes == NACL_IFLAG(OperandSize_b))) {
    NaClFatalInst(
        "Size implied by OperandSize_b, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_b");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeHasImmed_w)) &&
      (operand_sizes == NACL_IFLAG(OperandSize_w))) {
    NaClFatalInst(
        "Size implied by OperandSize_w, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_w");
  }
  if ((current_inst->flags & NACL_IFLAG(OpcodeHasImmed_v)) &&
      (operand_sizes == NACL_IFLAG(OperandSize_v))) {
    NaClFatalInst(
        "Size implied by OperandSize_v, use OpcodeHasImmed "
        "rather than OpcodeHasImmed_v");
  }
  if ((current_inst->flags & NACL_IFLAG(ModRmModIs0x3)) &&
      (NACL_EMPTY_IFLAGS == (current_inst->flags &
                             (NACL_IFLAG(OpcodeUsesModRm) |
                              NACL_IFLAG(OpcodeInModRm))))) {
    NaClFatalInst(
        "Can't specify ModRmModIs0x3 unless Opcode has modrm byte");
  }
  if ((current_inst->flags & NACL_IFLAG(ModRmModIs0x3)) &&
      (current_inst->flags & NACL_IFLAG(ModRmModIsnt0x3))) {
    NaClFatalInst(
        "Can't specify ModRmModIs0x3 and ModRmModIsnt0x3");
  }
}

static void NaClDefBytes(uint8_t opcode) {
  uint8_t index;
  current_inst->prefix = current_opcode_prefix;

  /* Start by clearing all entries. */
  for (index = 0; index < NACL_MAX_ALL_OPCODE_BYTES; ++index) {
    current_inst->opcode[index] = 0x00;
  }

  /* Now fill in non-final bytes. */
  index = 0;
  switch (current_opcode_prefix) {
    case NoPrefix:
      break;
    case Prefix0F:
    case Prefix660F:
    case PrefixF20F:
    case PrefixF30F:
      current_inst->opcode[0] = 0x0F;
      index = 1;
      break;
    case Prefix0F0F:
      current_inst->opcode[0] = 0x0F;
      current_inst->opcode[1] = 0x0F;
      index = 2;
      break;
    case Prefix0F38:
    case Prefix660F38:
    case PrefixF20F38:
      current_inst->opcode[0] = 0x0F;
      current_inst->opcode[1] = 0x38;
      index = 2;
      break;
    case Prefix0F3A:
    case Prefix660F3A:
      current_inst->opcode[0] = 0x0F;
      current_inst->opcode[1] = 0x3A;
      index = 2;
      break;
    case PrefixD8:
      current_inst->opcode[0] = 0xD8;
      index = 1;
      break;
    case PrefixD9:
      current_inst->opcode[0] = 0xD9;
      index = 1;
      break;
    case PrefixDA:
      current_inst->opcode[0] = 0xDA;
      index = 1;
      break;
    case PrefixDB:
      current_inst->opcode[0] = 0xDB;
      index = 1;
      break;
    case PrefixDC:
      current_inst->opcode[0] = 0xDC;
      index = 1;
      break;
    case PrefixDD:
      current_inst->opcode[0] = 0xDD;
      index = 1;
      break;
    case PrefixDE:
      current_inst->opcode[0] = 0xDE;
      index = 1;
      break;
    case PrefixDF:
      current_inst->opcode[0] = 0xDF;
      index = 1;
      break;
    default:
      NaClFatal("Unrecognized opcode prefix in NaClDefBytes");
      break;
  }

  /* Now add final byte. */
  current_inst->opcode[index] = opcode;
  current_inst->num_opcode_bytes = index + 1;
}

static void NaClPrintInstDescriptor(struct Gio* out,
                                    const NaClInstPrefix prefix,
                                    const int opcode,
                                    const int modrm_index) {
  if (NACL_NO_MODRM_OPCODE == modrm_index) {
    gprintf(out, "%s 0x%02x: ",
            NaClInstPrefixName(prefix), opcode);
  } else {
    gprintf(out, "%s 0x%02x /%x: ",
            NaClInstPrefixName(prefix), opcode, modrm_index);
  }
}

static void VerifyModRmOpcodeRange(NaClInstPrefix prefix,
                                   uint8_t opcode,
                                   uint8_t modrm_opcode) {
  if (modrm_opcode > NACL_NO_MODRM_OPCODE) {
    NaClPrintInstDescriptor(NaClLogGetGio(), prefix, opcode, modrm_opcode);
    NaClFatal("Illegal modrm opcode specification when defined opcode choices");
  }
}

void NaClDefPrefixInstMrmChoices_32_64(const NaClInstPrefix prefix,
                                       const uint8_t opcode,
                                       const uint8_t modrm_opcode,
                                       const int count_32,
                                       const int count_64) {
  VerifyModRmOpcodeRange(prefix, opcode, modrm_opcode);
  if (NaClInstCount[opcode][prefix][modrm_opcode] !=
      NACL_DEFAULT_CHOICE_COUNT) {
    NaClPrintInstDescriptor(NaClLogGetGio(), prefix, opcode, modrm_opcode);
    NaClFatal("Redefining NaClOpcode choice count");
  }
  if (NACL_FLAGS_run_mode == X86_32) {
    NaClInstCount[opcode][prefix][modrm_opcode] = count_32;
  } else if (NACL_FLAGS_run_mode == X86_64) {
    NaClInstCount[opcode][prefix][modrm_opcode] = count_64;
  }
}

void NaClDefPrefixInstChoices(const NaClInstPrefix prefix,
                              const uint8_t opcode,
                              const int count) {
  NaClDefPrefixInstChoices_32_64(prefix, opcode, count, count);
}

void NaClDefPrefixInstMrmChoices(const NaClInstPrefix prefix,
                                 const uint8_t opcode,
                                 const uint8_t modrm_opcode,
                                 const int count) {
  NaClDefPrefixInstMrmChoices_32_64(prefix, opcode, modrm_opcode,
                                         count, count);
}

void NaClDefPrefixInstChoices_32_64(const NaClInstPrefix prefix,
                                    const uint8_t opcode,
                                    const int count_32,
                                    const int count_64) {
  NaClDefPrefixInstMrmChoices_32_64(prefix, opcode, NACL_NO_MODRM_OPCODE,
                                    count_32, count_64);
}

/* Adds opcode flags corresponding to REP/REPNE flags if defined by
 * the prefix.
 */
static void NaClAddRepPrefixFlagsIfApplicable() {
  switch (current_opcode_prefix) {
    case Prefix660F:
    case Prefix660F38:
    case Prefix660F3A:
      current_inst->flags |= NACL_IFLAG(OpcodeAllowsData16) |
          NACL_IFLAG(SizeIgnoresData16);
      break;
    case PrefixF20F:
    case PrefixF20F38:
      current_inst->flags |= NACL_IFLAG(OpcodeAllowsRepne);
      break;
    case PrefixF30F:
      current_inst->flags |= NACL_IFLAG(OpcodeAllowsRep);
      break;
    default:
      break;
  }
}

/* Define the next opcode (instruction), initializing with
 * no operands.
 */
static void NaClDefInstInternal(
    const uint8_t opcode,
    const NaClInstType insttype,
    NaClIFlags flags,
    const NaClMnemonic name,
    Bool no_install) {
  /* TODO(karl) If we can deduce a more specific prefix that
   * must be used, due to the flags associated with the opcode,
   * then put on the more specific prefix list. This will make
   * the defining of instructions easier, in that the caller doesn't
   * need to specify the prefix to use.
   */
  int i;

  /* Before starting, install an opcode sequence if applicable. */
  if (NULL != current_cand_inst_node) {
    current_inst_node = current_cand_inst_node;
  }
  current_cand_inst_node = NULL;

  /* Before starting, expand appropriate implicit flag assumnptions. */
  if (flags & NACL_IFLAG(OpcodeLtC0InModRm)) {
    flags |= NACL_IFLAG(OpcodeInModRm) | NACL_IFLAG(ModRmModIsnt0x3);
  }

  DEBUG(NaClLog(LOG_INFO, "Define %s %"NACL_PRIx8": %s(%02x)\n",
                NaClInstPrefixName(current_opcode_prefix),
                opcode, NaClMnemonicName(name), name);
        NaClIFlagsPrint(NaClLogGetGio(), flags));

  if (NaClMnemonicEnumSize <= name) {
    NaClFatal("Badly defined mnemonic name");
  }

  if (kNaClInstTypeRange <= insttype) {
    NaClFatal("Badly defined inst type");
  }

  /* Create opcode and initialize */
  current_inst_mrm = (NaClMrmInst*) malloc(sizeof(NaClMrmInst));
  if (NULL == current_inst_mrm) {
    NaClFatal("NaClDefInst: malloc failed");
  }
  DEBUG(NaClLog(LOG_INFO,
                "current = %p\n", (void*) current_inst_mrm));
  current_inst_mrm->next = NULL;
  current_inst = &(current_inst_mrm->inst);
  NaClDefBytes(opcode);
  current_inst->insttype = insttype;
  current_inst->flags = NACL_EMPTY_IFLAGS;
  current_inst->name = name;
  current_inst->operands_desc = NULL;
  current_inst->next_rule = NULL;

  /* undefine all operands. */
  current_inst->num_operands = 0;
  for (i = 0; i < NACL_MAX_NUM_OPERANDS; ++i) {
    NaClDefOpInternal(Unknown_Operand, 0);
  }
  /* Now reset number of operands to zero. */
  current_inst->num_operands = 0;

  NaClAddIFlags(flags);

  NaClAddRepPrefixFlagsIfApplicable();

  NaClApplySanityChecksToInst();

  if (no_install || !NaClIFlagsMatchesRunMode(flags)) {
    return;
  }

  if (NULL == current_inst_node) {
    /* Install NaClOpcode. */
    DEBUG(NaClLog(LOG_INFO, "  standard install\n"));
    if (NULL == NaClInstTable[opcode][current_opcode_prefix]) {
      NaClInstTable[opcode][current_opcode_prefix] = current_inst;
    } else {
      NaClInst* next = NaClInstTable[opcode][current_opcode_prefix];
      while (NULL != next->next_rule) {
        next = next->next_rule;
      }
      next->next_rule = current_inst;
    }
    /* Install assuming no modrm opcode. Let NaClDefOp move if needed. */
    NaClInstallCurrentIntoOpcodeMrm(current_opcode_prefix, opcode,
                                NACL_NO_MODRM_OPCODE);
  } else if (NULL == current_inst_node->matching_inst) {
    DEBUG(NaClLog(LOG_INFO, "  instruction sequence install\n"));
    current_inst_node->matching_inst = current_inst;
  } else {
    NaClFatalInst(
        "Can't define more than one opcode for the same opcode sequence");
  }
}

void NaClDefInst(
    const uint8_t opcode,
    const NaClInstType insttype,
    NaClIFlags flags,
    const NaClMnemonic name) {
  NaClDefInstInternal(opcode, insttype, flags, name, FALSE);
}

/* Simple (fast hack) routine to extract a byte value from a character string.
 */
static int NaClExtractByte(const char* chars, const char* opcode_seq) {
  char buffer[3];
  int i;
  for (i = 0; i < 2; ++i) {
    char ch = *(chars++);
    if ('\0' == ch) {
      NaClLog(LOG_ERROR,
              "Odd number of characters in opcode sequence: '%s'\n",
              opcode_seq);
      NaClFatal("Fix before continuing!");
    }
    buffer[i] = ch;
  }
  buffer[2] = '\0';
  return strtoul(buffer, NULL, 16);
}

static NaClInstNode* NaClNewInstNode() {
  int i;
  NaClInstNode* root = (NaClInstNode*) malloc(sizeof(NaClInstNode));
  root->matching_inst = NULL;
  for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
    root->succs[i] = NULL;
  }
  return root;
}

/* Simple (fast hack) recursive routine to automatically install
 * an opcode sequence into the rooted trie.
 */
static NaClInstNode* NaClInstallInstSeq(int index,
                                        const char* opcode_seq,
                                        const char* chars_left,
                                        NaClInstNode** root_ptr) {
  NaClInstNode* root = *root_ptr;
  if (index > NACL_MAX_BYTES_PER_X86_INSTRUCTION) {
    NaClLog(LOG_ERROR,
            "Too many bytes specified for opcode sequence: '%s'\n",
            opcode_seq);
    NaClFatal("Fix before continuing!\n");
  }
  if (NULL == root) {
    root = NaClNewInstNode();
    *root_ptr = root;
  }
  if (*chars_left) {
    int byte = NaClExtractByte(chars_left, opcode_seq);
    return NaClInstallInstSeq(
        index+1, opcode_seq, chars_left + 2, &(root->succs[byte]));
  } else {
    return root;
  }
}

static void NaClDefInstSeq(const char* opcode_seq) {
  /* First check that opcode sequence not defined twice without a corresponding
   * call to NaClDefInst.
   */
  if (NULL != current_cand_inst_node) {
    NaClLog(LOG_ERROR,
            "Multiple definitions for opcode sequence: '%s'\n", opcode_seq);
    NaClFatal("Fix before continuing!");
  }
  /* Now install into lookup trie. */
  current_cand_inst_node =
      NaClInstallInstSeq(0, opcode_seq, opcode_seq, &inst_node_root);
}

void NaClAddOperandsDesc(const char* desc) {
  current_inst->operands_desc = strdup(desc);
}

/* Apply checks to current instruction flags, and update model as
 * appropriate.
 */
static void NaClRecheckIFlags() {
  if (!NaClIFlagsMatchesRunMode(current_inst->flags)) {
    NaClRemoveCurrentInstMrmFromInstTable();
    NaClRemoveCurrentInstMrmFromInstMrmTable();
  }
  /* If the instruction has an opcode in modrm, then it uses modrm. */
  if (NACL_EMPTY_IFLAGS != (current_inst->flags & NACL_IFLAG(OpcodeInModRm))) {
    current_inst->flags |= NACL_IFLAG(OpcodeUsesModRm);
  }
  /* If the instruction allows a two byte value, add DATA16 flag. */
  if (NACL_EMPTY_IFLAGS != (current_inst->flags &
                            (NACL_IFLAG(OperandSize_w) |
                             NACL_IFLAG(OpcodeHasImmed_z)))) {
    current_inst->flags |= NACL_IFLAG(OpcodeAllowsData16);
  }
  /* If the instruction uses the modrm rm field as an opcode value,
   * it also requires that the modrm mod field is 0x3.
   */
  if (current_inst->flags & NACL_IFLAG(OpcodeInModRmRm)) {
    current_inst->flags |= NACL_IFLAG(ModRmModIs0x3);
  }
  NaClApplySanityChecksToInst();
}

void NaClAddIFlags(NaClIFlags more_flags) {
  DEBUG(
      struct Gio* g = NaClLogGetGio();
      NaClLog(LOG_INFO, "Adding instruction flags: ");
      NaClIFlagsPrint(g, more_flags);
      gprintf(g, "\n"));
  current_inst->flags |= more_flags;
  NaClRecheckIFlags();
}

void NaClRemoveIFlags(NaClIFlags less_flags) {
  DEBUG(
      struct Gio* g = NaClLogGetGio();
      NaClLog(LOG_INFO, "Removing instruction flags: ");
      NaClIFlagsPrint(g, less_flags);
      gprintf(g, "\n"));
  current_inst->flags &= ~less_flags;
  NaClRecheckIFlags();
}

void NaClDelaySanityChecks() {
  apply_sanity_checks = FALSE;
}

void NaClApplySanityChecks() {
  apply_sanity_checks = TRUE;
  if (NULL != current_inst) {
    int i;
    NaClApplySanityChecksToInst();
    for (i = 0; i < current_inst->num_operands; i++) {
      NaClApplySanityChecksToOp(i);
    }
  }
}

static void NaClInitInstTables() {
  int i;
  NaClInstPrefix prefix;
  int j;
  /* Before starting, verify that we have defined NaClOpcodeFlags
   * and NaClOpFlags big enough to hold the flags associated with it.
   */
  assert(NaClIFlagEnumSize <= sizeof(NaClIFlags) * 8);
  assert(NaClOpFlagEnumSize <= sizeof(NaClOpFlags) * 8);
  assert(NaClDisallowsFlagEnumSize <= sizeof(NaClDisallowsFlags) * 8);

  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      NaClInstTable[i][prefix] = NULL;
      for (j = 0; j <= NACL_NO_MODRM_OPCODE; ++j) {
        NaClInstCount[i][prefix][j] = NACL_DEFAULT_CHOICE_COUNT;
      }
    }
    NaClPrefixTable[i] = "0";
  }
  NaClDefInstPrefix(NoPrefix);
  NaClDefInstInternal(0x0, NACLi_INVALID, 0, InstInvalid, TRUE);
  undefined_inst = current_inst;
}

static void NaClDefPrefixBytes() {
  NaClEncodePrefixName(kValueSEGES,  "kPrefixSEGES");
  NaClEncodePrefixName(kValueSEGCS,  "kPrefixSEGCS");
  NaClEncodePrefixName(kValueSEGSS,  "kPrefixSEGSS");
  NaClEncodePrefixName(kValueSEGDS,  "kPrefixSEGDS");
  NaClEncodePrefixName(kValueSEGFS,  "kPrefixSEGFS");
  NaClEncodePrefixName(kValueSEGGS,  "kPrefixSEGGS");
  NaClEncodePrefixName(kValueDATA16, "kPrefixDATA16");
  NaClEncodePrefixName(kValueADDR16, "kPrefixADDR16");
  NaClEncodePrefixName(kValueLOCK,   "kPrefixLOCK");
  NaClEncodePrefixName(kValueREPNE,  "kPrefixREPNE");
  NaClEncodePrefixName(kValueREP,    "kPrefixREP");

  if (NACL_FLAGS_run_mode == X86_64) {
    int i;
    for (i = 0; i < 16; ++i) {
      NaClEncodePrefixName(0x40+i, "kPrefixREX");
    }
  }
}

/* Define the given character sequence, associated with the given byte
 * opcode and instruction mnemonic, as a nop.
 */
static void NaClDefNopLikeSeq(const char* sequence, uint8_t opcode,
                              NaClMnemonic name ) {
  NaClDefInstSeq(sequence);
  NaClDefInst(opcode, NACLi_386, NACL_EMPTY_IFLAGS, name);
}

/* Define the given character sequence, associated with the given byte
 * opcode, as a nop.
 */
static void NaClDefNopSeq(const char* sequence, uint8_t opcode) {
  NaClDefNopLikeSeq(sequence, opcode, InstNop);
}

static void NaClDefNops() {
  /* nop */
  NaClDefNopSeq("90", 0x90);
  NaClDefNopSeq("6690", 0x90);
  NaClDefNopLikeSeq("f390", 0x90, InstPause);
  /* nop [%[re]ax] */
  NaClDefNopSeq("0f1f00", 0x1f);
  /* nop [%[re]ax+0] */
  NaClDefNopSeq("0f1f4000", 0x1f);
  /* nop [%[re]ax*1+0] */
  NaClDefNopSeq("0f1f440000", 0x1f);
  /* nop [%[re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("660f1f440000", 0x1f);
  /* nop [%[re]ax+0] */
  NaClDefNopSeq("0f1f8000000000", 0x1f);
  /* nop [%[re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("0f1f840000000000", 0x1f);
  /* nop [%[re]ax+%[re]ax+1+0] */
  NaClDefNopSeq("660f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("66662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("6666662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("666666662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("66666666662e0f1f840000000000", 0x1f);
  /* nop %cs:[%re]ax+%[re]ax*1+0] */
  NaClDefNopSeq("6666666666662e0f1f840000000000", 0x1f);
  /* UD2 */
  NaClDefNopLikeSeq("0f0b", 0x0b, InstUd2);
}

/* Build the set of x64 opcode (instructions). */
static void NaClBuildInstTables() {
  struct NaClSymbolTable* st = NaClSymbolTableCreate(5, NULL);

  /* Create common (global) symbol table with instruction set presumptions. */
  NaClSymbolTablePutText(
      "sp", ((X86_32 == NACL_FLAGS_run_mode) ? "esp" : "rsp"), st);
  NaClSymbolTablePutText(
      "ip", ((X86_32 == NACL_FLAGS_run_mode) ? "eip" : "rip"), st);
  NaClSymbolTablePutText(
      "bp", ((X86_32 == NACL_FLAGS_run_mode) ? "ebp" : "rbp"), st);

  NaClInitInstTables();
  NaClDefPrefixBytes();
  NaClDefOneByteInsts(st);
  NaClDef0FInsts(st);
  NaClDefSseInsts(st);
  NaClDefX87Insts(st);
  NaClDefNops();

  NaClSymbolTableDestroy(st);
}

/* Return the number of instruction rules pointed to by the parameter. */
static int NaClInstListLength(NaClInst* next) {
  int count = 0;
  while (NULL != next) {
    ++count;
    next = next->next_rule;
  }
  return count;
}

static int NaClInstMrmListLength(NaClMrmInst* next) {
  int count = 0;
  while (NULL != next) {
    ++count;
    next = next->next;
  }
  return count;
}

/* Collect the number of opcode (instructions) in the given table,
 * and return the number found.
 */
static int NaClCountNumberInsts() {
  int i;
  NaClInstPrefix prefix;
  int count = 0;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      count += NaClInstListLength(NaClInstTable[i][prefix]);
    }
  }
  return count;
}

static void NaClFatalChoiceCount(const int expected,
                                 const int found,
                                 const NaClInstPrefix prefix,
                                 const int opcode,
                                 const int modrm_index,
                                 NaClMrmInst* insts) {
  struct Gio* g = NaClLogGetGio();
  NaClPrintInstDescriptor(g, prefix, opcode, modrm_index);
  NaClLog(LOG_ERROR, "Expected %d rules but found %d:\n", expected, found);
  while (NULL != insts) {
    NaClInstPrint(g, &(insts->inst));
    insts = insts->next;
  }
  NaClFatal("fix before continuing...\n");
}

/* Verify that the number of possible choies for each prefix:opcode matches
 * what was explicitly defined.
 */
static void NaClVerifyInstCounts() {
  int i, j;
  NaClInstPrefix prefix;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      for (j = 0; j < NACL_MODRM_OPCODE_SIZE; ++j) {
        NaClMrmInst* insts = NaClInstMrmTable[i][prefix][j];
        int found = NaClInstMrmListLength(insts);
        int expected = NaClInstCount[i][prefix][j];
        if (expected == NACL_DEFAULT_CHOICE_COUNT) {
          if (found > 1) {
            NaClFatalChoiceCount(1, found, prefix, i, j, insts);
          }
        } else if (expected != found) {
          NaClFatalChoiceCount(expected, found, prefix, i, j, insts);
        }
      }
    }
  }
}

/* Removes X86-32 specific flags from the given instruction. */
static void NaClInstRemove32Stuff(NaClInst* inst) {
  inst->flags &= ~(NACL_IFLAG(Opcode32Only));
}

/* Removes X86-64 specific flags from the given instruction. */
static void NaClInstRemove64Stuff(NaClInst* inst) {
  inst->flags &=
      ~(NACL_IFLAG(OpcodeRex) |
        NACL_IFLAG(OpcodeUsesRexW) | NACL_IFLAG(OpcodeHasRexR) |
        NACL_IFLAG(Opcode64Only) | NACL_IFLAG(OperandSize_o) |
        NACL_IFLAG(AddressSize_o) | NACL_IFLAG(OperandSizeDefaultIs64) |
        NACL_IFLAG(OperandSizeForce64));
}

/* Simplifies the instructions if possible. Mostly removes flags that
 * don't correspond to the run mode.
 */
static void NaClSimplifyIfApplicable() {
  int i;
  for (i = 0; i < NCDTABLESIZE; ++i) {
    NaClInstPrefix prefix;
    for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
      NaClInst* next = NaClInstTable[i][prefix];
      while (NULL != next) {
        if (X86_64 != NACL_FLAGS_run_mode) {
          NaClInstRemove64Stuff(next);
        }
        if (X86_32 != NACL_FLAGS_run_mode) {
          NaClInstRemove32Stuff(next);
        }
        /* Remove size only flags, since already compiled into tables. */
        next->flags &= ~(NACL_IFLAG(Opcode64Only) | NACL_IFLAG(Opcode32Only));
        next = next->next_rule;
      }
    }
  }
}

/* Prints out the set of defined instruction flags. */
static void NaClIFlagsPrintInternal(struct Gio* f, NaClIFlags flags) {
  if (flags) {
    int i;
    Bool first = TRUE;
    for (i = 0; i < NaClIFlagEnumSize; ++i) {
      if (flags & NACL_IFLAG(i)) {
        if (first) {
          first = FALSE;
        } else {
          gprintf(f, " | ");
        }
        gprintf(f, "NACL_IFLAG(%s)", NaClIFlagName(i));
      }
    }
  } else {
    gprintf(f, "NACL_EMPTY_IFLAGS");
  }
}

/* Prints out the set of defined Operand flags. */
static void NaClOpFlagsPrintInternal(struct Gio* f, NaClOpFlags flags) {
  if (flags) {
    NaClOpFlag i;
    Bool first = TRUE;
    for (i = 0; i < NaClOpFlagEnumSize; ++i) {
      if (flags & NACL_OPFLAG(i)) {
        if (first) {
          first = FALSE;
        } else {
          gprintf(f, " | ");
        }
        gprintf(f, "NACL_OPFLAG(%s)", NaClOpFlagName(i));
      }
    }
  } else {
    gprintf(f, "NACL_EMPTY_OPFLAGS");
  }
}

/* Print out the opcode operand. */
static void NaClOpPrintInternal(struct Gio* f, NaClOp* operand) {
  gprintf(f, "{ %s, ", NaClOpKindName(operand->kind));
  NaClOpFlagsPrintInternal(f, operand->flags);
  gprintf(f, " },\n");
}

/* Prints out the given instruction to the given file. If index >= 0,
 * print out a comment, with the value of index, before the printed
 * instruction. Lookahead is used to convert the next_rule pointer into
 * a symbolic reference using the name "g_Opcodes", plus the index defined by
 * the lookahead. Argument as_array_element is true if the element is
 * assumed to be in an array static initializer.
 */
static void NaClInstPrintInternal(struct Gio* f, Bool as_array_element,
                                  int index, NaClInst* inst, int lookahead) {
  int i;
  gprintf(f, "  /* %d */\n", index);
  gprintf(f, "  { %s,", NaClInstPrefixName(inst->prefix));
  gprintf(f, " %"NACL_PRIu8", {", inst->num_opcode_bytes);
  for (i = 0; i < NACL_MAX_ALL_OPCODE_BYTES; ++i) {
    if (i > 0) gprintf(f, ",");
    gprintf(f," 0x%02x", inst->opcode[i]);
  }
  gprintf(f, " }, %s,\n", kNaClInstTypeString[inst->insttype]);
  gprintf(f, "    ");
  NaClIFlagsPrintInternal(f, inst->flags);
  gprintf(f, ",\n");
  gprintf(f, "    Inst%s,\n", NaClMnemonicName(inst->name));
  gprintf(f, "    %u, {\n", inst->num_operands);
  for (i = 0; i < NACL_MAX_NUM_OPERANDS; ++i) {
    gprintf(f, "      ");
    NaClOpPrintInternal(f, inst->operands + i);
  }
  gprintf(f, "    },\n");
  gprintf(f, "    \"%s\",\n", inst->operands_desc);
  if (index < 0 || NULL == inst->next_rule) {
    gprintf(f, "    NULL\n");
  } else {
    gprintf(f, "    g_Opcodes + %d\n", lookahead);
  }
  if (as_array_element) {
    gprintf(f, "  }%s\n", index < 0 ? ";" : ",");
  } else {
    gprintf(f, "  };\n");
  }
}

/* Prints out the given instruction to the given file. If index >= 0,
 * print out a comment, with the value of index, before the printed
 * instruction Lookahead is used to convert the next_rule pointer into
 * a symbolic reference using the name "g_Opcodes", plus the index and
 * the lookahead.
 */
void NaClInstPrintTablegen(struct Gio* f, int index,
                           NaClInst* inst, int lookahead) {
  NaClInstPrintInternal(f, TRUE, index, inst, lookahead);
}

/* Generate header information, based on the executable name in argv0,
 * and the file to be generated (defined by fname).
 */
static void NaClPrintHeader(struct Gio* f, const char* argv0,
                            const char* fname) {
  time_t timeofday;
  if (time(&timeofday) < 0) {
    NaClFatal("time() failed\n");
  }
  gprintf(f, "/* %s\n", fname);
  gprintf(f, " * THIS FILE IS AUTO-GENERATED. DO NOT EDIT.\n");
  gprintf(f, " * This file was auto-generated by %s\n", argv0);
  gprintf(f, " * on: %s\n", ctime(&timeofday));
  gprintf(f, " *\n");
  gprintf(f, " * Compiled for %s.\n", NaClRunModeName(NACL_FLAGS_run_mode));
  gprintf(f, " *\n");
  gprintf(f, " * You must include ncopcode_desc.h before this file.\n");
  gprintf(f, " */\n\n");
}

/* Print out which bytes correspond to prefix bytes. */
static void NaClPrintPrefixTable(struct Gio* f) {
  int opc;
  gprintf(f, "static const uint32_t kNaClPrefixTable[NCDTABLESIZE] = {");
  for (opc = 0; opc < NCDTABLESIZE; opc++) {
    if (0 == opc % 16) {
      gprintf(f, "\n  /* 0x%02x-0x%02x */\n  ", opc, opc + 15);
    }
    gprintf(f, "%s, ", NaClPrefixTable[opc]);
  }
  gprintf(f, "\n};\n\n");
}

static int NaClCountInstSeqs(NaClInstNode* root) {
  if (root == NULL) {
    return 0;
  } else {
    int count = 0;
    int index;
    if (NULL != root->matching_inst) count++;
    for (index = 0; index < NACL_NUM_BYTE_VALUES; ++index) {
      count += NaClCountInstSeqs(root->succs[index]);
    }
    return count;
  }
}

static int NaClCountInstNodes(NaClInstNode* root) {
  if (NULL == root) {
    return 0;
  } else {
    int count = 1;
    int index;
    for (index = 0; index < NACL_NUM_BYTE_VALUES; ++index) {
      count += NaClCountInstNodes(root->succs[index]);
    }
    return count;
  }
}

static void NaClPrintInstTrieNode(NaClInstNode* root, int g_opcode_index,
                                  int root_index, struct Gio* f) {
  if (NULL == root) {
    return;
  } else {
    int i = 0;
    int next_index = root_index + 1;
    gprintf(f, "  /* %d */\n", root_index);
    gprintf(f, "  { ");
    if (NULL == root->matching_inst) {
      gprintf(f, "NULL");
    } else {
      gprintf(f, "g_Opcodes + %d", g_opcode_index);
    }
    gprintf(f, ", {\n");
    for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
      gprintf(f, "    /* %02x */ ", i);
      if (NULL == root->succs[i]) {
        gprintf(f, "NULL");
      } else {
        gprintf(f, "g_OpcodeSeq + %d", next_index);
        next_index += NaClCountInstNodes(root->succs[i]);
      }
      gprintf(f, ",\n");
    }
    gprintf(f, "    } },\n");
    next_index = root_index + 1;
    for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
      NaClPrintInstTrieNode(root->succs[i], g_opcode_index, next_index, f);
      g_opcode_index += NaClCountInstSeqs(root->succs[i]);
      next_index += NaClCountInstNodes(root->succs[i]);
    }
  }
}

/* Prints out the contents of the opcode sequence overrides into the
 * given file.
 */
static void NaClPrintInstSeqTrie(int g_opcode_index,
                                 NaClInstNode* root,
                                 struct Gio* f) {
  /* Make sure trie isn't empty, since empty arrays create warning messages. */
  int num_trie_nodes;
  if (root == NULL) root = NaClNewInstNode();
  num_trie_nodes = NaClCountInstNodes(root);
  gprintf(f, "static NaClInstNode g_OpcodeSeq[%d] = {\n", num_trie_nodes);
  NaClPrintInstTrieNode(root, g_opcode_index, 0, f);
  gprintf(f, "};\n");
}

static void NaClPrintInstsInInstTrie(int current_index,
                                     NaClInstNode* root,
                                     struct Gio* f) {
  int index;
  if (NULL == root) return;
  if (NULL != root->matching_inst) {
    NaClInstPrintInternal(f, TRUE, current_index, root->matching_inst, 0);
    current_index++;
  }
  for (index = 0; index < NACL_NUM_BYTE_VALUES; ++index) {
    NaClPrintInstsInInstTrie(current_index, root->succs[index], f);
    current_index += NaClCountInstSeqs(root->succs[index]);
  }
}

/* Print out the contents of the defined instructions into the given file. */
static void NaClPrintDecodeTables(struct Gio* f) {
  int i;
  NaClInstPrefix prefix;
  int count = 0;

  /* lookup_index holds the number of the opcode (instruction) that
   * begins the list of instructions for the corresponding opcode
   * in NaClOpcodeTable.
   */
  int lookup_index[NCDTABLESIZE][NaClInstPrefixEnumSize];

  /* Build table of all possible instructions. Note that we build
   * build the list of instructions by using a "g_NaClOpcodes" address.
   */
  int num_opcodes = NaClCountNumberInsts();
  int num_seq_opcodes = NaClCountInstSeqs(inst_node_root);

  gprintf(f,
          "static NaClInst g_Opcodes[%d] = {\n",
          num_opcodes + num_seq_opcodes);
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    for (i = 0; i < NCDTABLESIZE; ++i) {
      /* Build the list of instructions by knowing that the next
       * element in the list will always follow the current entry,
       * when added to the array.
       */
      NaClInst* next = NaClInstTable[i][prefix];
      if (NULL == next) {
        lookup_index[i][prefix] = -1;
      } else {
        int lookahead = count + 1;
        lookup_index[i][prefix] = count;
        while (NULL != next) {
          NaClInstPrintTablegen(f, count, next, lookahead);
          next = next->next_rule;
          ++lookahead;
          ++count;
        }
      }
    }
  }
  NaClPrintInstsInInstTrie(num_opcodes, inst_node_root, f);
  gprintf(f, "};\n\n");

  /* Print out the undefined opcode */
  gprintf(f, "static NaClInst g_Undefined_Opcode = \n");
  NaClInstPrintInternal(f, FALSE, 0, undefined_inst, 0);

  /* Now print lookup table of rules. */
  gprintf(f,
          "static NaClInst* "
          "g_OpcodeTable[NaClInstPrefixEnumSize][NCDTABLESIZE] = {\n");
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    gprintf(f,"/* %s */\n", NaClInstPrefixName(prefix));
    gprintf(f, "{\n");
    for (i = 0; i < NCDTABLESIZE; ++i) {
      /* Take advantage of the fact that the lists were added to
       * the array of opcodes such that the next element in the list
       * will always follow the current entry.
       */
      NaClInst* next = NaClInstTable[i][prefix];
      gprintf(f, "  /* %02x */ ", i);
      if (NULL == next) {
        gprintf(f, "NULL");
      } else {
        gprintf(f, "g_Opcodes + %d", lookup_index[i][prefix]);
      }
      gprintf(f, "  ,\n");
    }
    gprintf(f, "},\n");
  }
  gprintf(f, "};\n\n");

  NaClPrintPrefixTable(f);

  NaClPrintInstSeqTrie(num_opcodes, inst_node_root, f);
}

/* Print out the sequence of bytes used to encode an instruction sequence. */
static void PrintInstructionSequence(
    struct Gio* f, uint8_t* inst_sequence, int length) {
  int i;
  for (i = 0; i < length; ++i) {
    if (i > 0) gprintf(f, " ");
    gprintf(f, "%02x", inst_sequence[i]);
  }
}

/* Print out instruction sequences defined for the given (Trie) node,
 * and all of its descendants.
 * Note: To keep recursive base cases simple, we allow one more byte
 * in the instruction sequence that is actually possible.
 */
static void PrintHardCodedInstructionsNode(
    struct Gio* f, NaClInstNode* node,
    uint8_t inst_sequence[NACL_MAX_BYTES_PER_X86_INSTRUCTION+1], int length) {
  int i;
  if (NULL == node) return;
  if (NACL_MAX_BYTES_PER_X86_INSTRUCTION < length) {
    struct Gio* glog = NaClLogGetGio();
    NaClLog(LOG_ERROR, "%s", "");
    PrintInstructionSequence(glog, inst_sequence, length);
    gprintf(glog, "\n");
    NaClFatal("Hard coded instruction too long, aborting\n");
  }
  if (NULL != node->matching_inst) {
    gprintf(f, "  --- ");
    PrintInstructionSequence(f, inst_sequence, length);
    gprintf(f, " ---\n");
    NaClInstPrint(f, node->matching_inst);
    gprintf(f, "\n");
  }
  for (i = 0; i < 256; ++i) {
    inst_sequence[length] = (uint8_t) i;
    PrintHardCodedInstructionsNode(f, node->succs[i], inst_sequence, length+1);
  }
}

/* Walks over specifically encoded instruction trie, and prints
 * out corresponding implemented instructions.
 */
static void NaClPrintHardCodedInstructions(struct Gio* f) {
  uint8_t inst_sequence[NACL_MAX_BYTES_PER_X86_INSTRUCTION+1];
  PrintHardCodedInstructionsNode(f, inst_node_root, inst_sequence, 0);
  PrintHardCodedInstructionsNode(f, NULL, inst_sequence, 0);
}

/* Prints out documentation on the modeled instruction set. */
static void NaClPrintInstructionSet(struct Gio* f) {
  NaClInstPrefix prefix;
  int i;
  gprintf(f, "Target: %s\n", NaClRunModeName(NACL_FLAGS_run_mode));
  gprintf(f, "\n");
  gprintf(f, "*** Hard coded instructions ***\n");
  gprintf(f, "\n");
  NaClPrintHardCodedInstructions(f);

  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize;  ++prefix) {
    Bool printed_rules = FALSE;
    gprintf(f, "*** %s ***\n", NaClInstPrefixName(prefix));
    gprintf(f, "\n");
    for (i = 0; i < NCDTABLESIZE; ++i) {
      Bool is_first = TRUE;
      NaClInst* inst = NaClInstTable[i][prefix];
      while (NULL != inst) {
        if (is_first) {
          gprintf(f, "  --- %02x ---\n", i);
          is_first = FALSE;
        }
        NaClInstPrint(f, inst);
        printed_rules = TRUE;
        inst = inst->next_rule;
      }
    }
    if (printed_rules) gprintf(f, "\n");
  }
}

/* Open the given file using the given directives (how). */
static void NaClMustOpen(struct GioFile* g,
                         const char* fname, const char* how) {
  if (!GioFileCtor(g, fname, how)) {
    NaClLog(LOG_ERROR, "could not fopen(%s, %s)\n", fname, how);
    NaClFatal("exiting now");
  }
}

/* Recognizes flags in argv, processes them, and then removes them.
 * Returns the updated value for argc.
 */
static int NaClGrokFlags(int argc, const char* argv[]) {
  int i;
  int new_argc;
  if (argc == 0) return 0;
  new_argc = 1;
  for (i = 1; i < argc; ++i) {
    if (0 == strcmp("-m32", argv[i])) {
      NACL_FLAGS_run_mode = X86_32;
    } else if (0 == strcmp("-m64", argv[i])) {
      NACL_FLAGS_run_mode = X86_64;
    } else if (GrokBoolFlag("-documentation", argv[i],
                            &NACL_FLAGS_human_readable)) {
      continue;
    } else {
      argv[new_argc++] = argv[i];
    }
  }
  return new_argc;
}

static void GenerateTables(struct Gio* f, const char* cmd,
                           const char* filename) {
  if (NACL_FLAGS_human_readable) {
    NaClPrintInstructionSet(f);
  } else {
    /* Generate header file defining instruction set. */
    NaClPrintHeader(f, cmd, filename);
    NaClPrintDecodeTables(f);
  }
}

/* Walk the trie of explicitly defined opcode sequences, and
 * fill in the operands description field if it hasn't been
 * explicitly defined.
 */
static void FillInTrieMissingOperandsDescs(NaClInstNode* node) {
  int i;
  NaClInst* inst;
  if (NULL == node) return;
  inst = node->matching_inst;
  if ((NULL != inst) && (NULL == inst->operands_desc)) {
    inst->operands_desc = NaClDefaultOperandsDesc(inst);
  }
  for (i = 0; i < NACL_NUM_BYTE_VALUES; ++i) {
    FillInTrieMissingOperandsDescs(node->succs[i]);
  }
}

/* Define the operands description field of each modeled
 * opcode instruction, if it hasn't explicitly been defined.
 */
static void FillInMissingOperandsDescs() {
  int i;
  NaClInstPrefix prefix;
  for (prefix = NoPrefix; prefix < NaClInstPrefixEnumSize; ++prefix) {
    for (i = 0; i < NCDTABLESIZE; ++i) {
      NaClInst* next = NaClInstTable[i][prefix];
      while (NULL != next) {
        if (NULL == next->operands_desc) {
          next->operands_desc = NaClDefaultOperandsDesc(next);
        }
        next = next->next_rule;
      }
    }
  }
  FillInTrieMissingOperandsDescs(inst_node_root);
}

int main(const int argc, const char* argv[]) {
  struct GioFile gfile;
  struct Gio* g = (struct Gio*) &gfile;
  int new_argc = NaClGrokFlags(argc, argv);
  if ((new_argc < 1) || (new_argc > 2) ||
      (NACL_FLAGS_run_mode == NaClRunModeSize)) {
    fprintf(stderr,
            "ERROR: usage: ncdecode_tablegen <architecture_flag> "
            "[-documentation] [file]\n");
    return -1;
  }

  NaClLogModuleInit();
  NaClBuildInstTables();
  NaClSimplifyIfApplicable();
  NaClVerifyInstCounts();
  FillInMissingOperandsDescs();

  if (new_argc == 1) {
    GioFileRefCtor(&gfile, stdout);
    GenerateTables(g, argv[0], "<stdout>");
  } else {
    NaClMustOpen(&gfile, argv[1], "w");
    GenerateTables(g, argv[0], argv[1]);
  }
  NaClLogModuleFini();
  GioFileDtor(g);
  return 0;
}
