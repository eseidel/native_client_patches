/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_H__

/*
 * ncvalidate_iter.h
 * Type declarations of the iterator state etc.
 *
 * This is the interface to the iterator form of the NaCL validator.
 * Basic usage:
 *   -- base is initial address of ELF file.
 *   -- limit is the size of the ELF file.
 *   -- maddr is the address to the memory of a section.
 *   -- vaddr is the starting virtual address associated with a section.
 *   -- size is the number of bytes in a section.
 *
 *   NaClValidatorState* state =
 *     NaClValidatorStateCreate(base, limit - base, 32, RegR15);
 *   if (state == NULL) fail;
 *   for each section:
 *     NaClValidateSegment(maddr, vaddr, size, state);
 *   if (!NaClValidatesOk(state)) fail;
 *   NaClValidatorStateDestroy(state);
 */

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state.h"

/* Control flag that when set to FALSE, turns of the printing of validator
 * messages.
 */
extern Bool NACL_FLAGS_print_validator_messages;

/* When >= 0, only print this many errors before quiting. When
 * < 0, print all errors.
 */
extern int NACL_FLAGS_max_reported_errors;

/* Command line flag controlling whether each instruction is traced
 * while validating instructions.
 */
extern Bool NACL_FLAGS_validator_trace_instructions;

/* Command line flag controlling whether the internal representation
 * of each instruction is trace while validating.
 * Command line flag controlling whether each instruction, and its
 * corresponding internal details, is traced while validating
 * instructions.
 */
extern Bool NACL_FLAGS_validator_trace_inst_interals;

/* Define the stop instruction. */
extern const uint8_t kNaClFullStop;

/* Changes all validator trace flags to true. */
void NaClValidatorFlagsSetTraceVerbose();

/* The model of a validator state. */
typedef struct NaClValidatorState NaClValidatorState;

/* Create a validator state to validate the ELF file with the given parameters.
 * Note: Messages (if any) produced by the validator are sent to the stream
 * defined by native_client/src/shared/platform/nacl_log.h.
 * Parameters.
 *   vbase - The virtual address for the contents of the ELF file.
 *   sz - The number of bytes in the ELF file.
 *   alignment: 16 or 32, specifying alignment.
 *   base_register - OperandKind defining value for base register (or
 *     RegUnknown if not defined).
 * Returns:
 *   A pointer to an initialized validator state if everything is ok, NULL
 *  otherwise.
 */
NaClValidatorState* NaClValidatorStateCreate(const NaClPcAddress vbase,
                                             const NaClMemorySize sz,
                                             const uint8_t alignment,
                                             const NaClOpKind base_register);

/* Returns the current maximum number of errors that can be reported.
 * Note: When > 0, the validator will only print that many errors before
 * quiting. When 0, the validator will not print any messages. When < 0,
 * the validator will print all found errors.
 * Note: Defaults to NACL_FLAGS_max_reported_errors.
 */
int NaClValidatorStateGetMaxReportedErrors(NaClValidatorState* state);

/* Changes the current maximum number of errors that will be reported before
 * quiting. For legal parameter values, see
 * NaClValidatorStateGetMaxReportedErrors.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 */
void NaClValidatorStateSetMaxReportedErrors(NaClValidatorState* state,
                                            int max_reported_errors);

/* Returns true if an opcode histogram should be printed by the validator.
 * Note: Defaults to NACL_FLAGS_opcode_histogram.
 */
Bool NaClValidatorStateGetPrintOpcodeHistogram(NaClValidatorState* state);

/* Changes the value on whether the opcode histogram should be printed by
 * the validator.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 */
void NaClValidatorStateSetPrintOpcodeHistogram(NaClValidatorState* state,
                                               Bool new_value);

/* Returns true if each instruction should be printed as the validator
 * processes the instruction.
 * Note: Defaults to NACL_FLAGS_validator_trace.
 */
Bool NaClValidatorStateGetTraceInstructions(NaClValidatorState* state);

/* Changes the value on whether each instruction should be printed as
 * the validator processes the instruction.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 */
void NaClValidatorStateSetTraceInstructions(NaClValidatorState* state,
                                            Bool new_value);

/* Returns true if the internal representation of each instruction
 * should be printed as the validator processes the instruction.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 */
Bool NaClValidatorStateGetTraceInstInternals(NaClValidatorState* state);

/* Changes the value on whether the internal details of each validated
 * instruction should be printed, as the validator visits the instruction.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 */
void NaClValidatorStateSetTraceInstInternals(NaClValidatorState* state,
                                             Bool new_value);

/* Returns true if any of thevalidator trace flags are set.
 * Note: If this function returns true, so does
 *    NaClValidatorStateGetTraceInstructions
 *    NaClValidatorStateGetTraceInstInternals
 */
Bool NaClValidatorStateTrace(NaClValidatorState* state);

/* Convenience function that changes all validator trace flags to true.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 */
void NaClValidatorStateSetTraceVerbose(NaClValidatorState* state);

/* Returns the log verbosity for printed validator messages. Legal
 * values are defined by src/shared/platform/nacl_log.h.
 * Note: Defaults to LOG_INFO.
 */
int NaClValidatorStateGetLogVerbosity(NaClValidatorState* state);

/* Changes the log verbosity for printed validator messages to the
 * new value. Legal values are defined by src/shared/platform/nacl_log.h.
 * Note: Should only be called between calls to NaClValidatorStateCreate
 * and NaClValidateSegment.
 * Note: NaClLogGetVerbosity() can override this value if more severe
 * than the value defined here. This allows a global limit (defined
 * by nacl_log.h) as well as a validator specific limit.
 */
void NaClValidatorStateSetLogVerbosity(NaClValidatorState* state,
                                       Bool new_value);

/* Return the value of the "do stub out" flag, i.e. whether instructions will
 * be stubbed out with HLT if they are found to be illegal.
 */
Bool NaClValidatorStateGetDoStubOut(NaClValidatorState* state);

/* Changes the "do stub out" flag to the given value. Note: Should only
 * be called between calls to NaClValidatorStateCreate and NaClValidateSegment.
 */
void NaClValidatorStateSetDoStubOut(NaClValidatorState* state,
                                    Bool new_value);

/* Validate a code segment.
 * Parameters:
 *   mbase - The address of the beginning of the code segment.
 *   vbase - The virtual address associated with the beginning of the code
 *       segment.
 *   sz - The number of bytes in the code segment.
 *   state - The validator state to use while validating.
 */
void NaClValidateSegment(uint8_t* mbase,
                         NaClPcAddress vbase,
                         NaClMemorySize sz,
                         NaClValidatorState* state);

/* Returns true if the validator hasn't found any problems with the validated
 * code segments.
 * Parameters:
 *   state - The validator state used to validate code segments.
 * Returns:
 *   true only if no problems have been found.
 */
Bool NaClValidatesOk(NaClValidatorState* state);

/* Cleans up and returns the memory created by the corresponding
 * call to NaClValidatorStateCreate.
 */
void NaClValidatorStateDestroy(NaClValidatorState* state);

/* Defines a function to create local memory to be used by a validator
 * function, should it need it.
 * Parameters:
 *   state - The state of the validator.
 * Returns:
 *   Allocated space for local data associated with a validator function.
 */
typedef void* (*NaClValidatorMemoryCreate)(NaClValidatorState* state);

/* Defines a validator function to be called on each instruction.
 * Parameters:
 *   state - The state of the validator.
 *   iter - The instruction iterator's current position in the segment.
 *   local_memory - Pointer to local memory generated by the corresponding
 *          NaClValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NaClValidator)(NaClValidatorState* state,
                              NaClInstIter* iter,
                              void* local_memory);

/* Defines a post validator function that is called after iterating through
 * a segment, but before the iterator is destroyed.
 * Parameters:
 *   state - The state of the validator,
 *   iter - The instruction iterator's current position in the segment.
 *   local_memory - Pointer to local memory generated by the corresponding
 *          NaClValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NaClValidatorPostValidate)(NaClValidatorState* state,
                                          NaClInstIter* iter,
                                          void* local_memory);

/* Defines a statistics print routine for a validator function.
 * Note: statistics will be printed to (nacl) log file.
 * Parameters:
 *   state - The state of the validator,
 *   local_memory - Pointer to local memory generated by the corresponding
 *          NaClValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NaClValidatorPrintStats)(NaClValidatorState* state,
                                        void* local_memory);

/* Defines a function to destroy local memory used by a validator function,
 * should it need to do so.
 * Parameters:
 *   state - The state of the validator.
 *   local_memory - Pointer to local memory generated by the corresponding
 *         NaClValidatorMemoryCreate (or NULL if not specified).
 */
typedef void (*NaClValidatorMemoryDestroy)(NaClValidatorState* state,
                                           void* local_memory);

/* Registers a validator function to be called during validation.
 * Parameters are:
 *   state - The state to register the validator functions in.
 *   validator - The validator function to register.
 *   post_validate - Validate global context after iterator run.
 *   print_stats - The print function to print statistics about the applied
 *     validator.
 *   memory_create - The function to call to generate local memory for
 *     the validator function (or NULL if no local memory is needed).
 *   memory_destroy - The function to call to reclaim local memory when
 *     the validator state is destroyed (or NULL if reclamation is not needed).
 */
void NaClRegisterValidator(NaClValidatorState* state,
                           NaClValidator validator,
                           NaClValidatorPostValidate post_validate,
                           NaClValidatorPrintStats print_stats,
                           NaClValidatorMemoryCreate memory_create,
                           NaClValidatorMemoryDestroy memory_destroy);

/* Returns the local memory associated with the given validator function,
 * or NULL if no such memory exists. Allows validators to communicate
 * shared collected information.
 * Parameters:
 *   validator - The validator function's memory you want access to.
 *   state - The current state of the validator.
 * Returns:
 *   The local memory associated with the validator (or NULL  if no such
 *   validator is known).
 */
void* NaClGetValidatorLocalMemory(NaClValidator validator,
                                  const NaClValidatorState* state);

/* Prints out a validator message for the given level.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   format - The format string of the message to print.
 *   ... - arguments to the format string.
 */
void NaClValidatorMessage(int level,
                          NaClValidatorState* state,
                          const char* format,
                          ...) ATTRIBUTE_FORMAT_PRINTF(3, 4);

/* Prints out a validator message for the given level using
 * a variable argument list.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   format - The format string of the message to print.
 *   ap - variable argument list for the format.
 */
void NaClValidatorVarargMessage(int level,
                                NaClValidatorState* state,
                                const char* format,
                                va_list ap);

/* Prints out a validator message for the given address.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   addr - The address where the error occurred.
 *   format - The format string of the message to print.
 *   ... - arguments to the format string.
 */
void NaClValidatorPcAddressMessage(int level,
                                   NaClValidatorState* state,
                                   NaClPcAddress addr,
                                   const char* format,
                                   ...) ATTRIBUTE_FORMAT_PRINTF(4, 5);

/* Prints out a validator message for the given instruction.
 * Parameters:
 *   level - The level of the message, as defined in nacl_log.h
 *   state - The validator state that detected the error.
 *   inst - The instruction that caused the vaidator error.
 *   format - The format string of the message to print.
 *   ... - arguments to the format string.
 */
void NaClValidatorInstMessage(int level,
                              NaClValidatorState* state,
                              NaClInstState* inst,
                              const char* format,
                              ...) ATTRIBUTE_FORMAT_PRINTF(4, 5);

/* Returns true if the validator should quit due to previous errors. */
Bool NaClValidatorQuit(NaClValidatorState* state);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_H__ */
