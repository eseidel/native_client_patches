#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

# This script is a replacement for llvm-gcc and llvm-g++ driver.
# It detects automatically which role it is supposed to assume.
# The point of the script is to redirect builds through our own tools,
# while making these tools appear like gnu tools.
#
# IMPORTANT NOTE: If you make local mods to this file, you must run:
#   %  tools/llvm/utman.sh driver
# in order for them to take effect in the scons build.  This command
# updates the copy in the toolchain/ tree.
#
# The code is broken down into sections:
#   1) Environment Settings
#   2) Patterns for command-line arguments
#   3) Chain Map
#   4) Main and Incarnations
#   5) Compiling / Linking Engine
#   6) Chain Logic
#   7) Environment
#   8) Argument Parsing
#   9) File Naming Logic
#   10) Shell Utilities
#   11) Logging
#   12) TLS Hack
#   13) Invocation

import os
import re
import subprocess
import sys

######################################################################
# Environment Settings
######################################################################

# This dictionary initializes a shell-like environment.
# Shell escaping and ${} substitution are provided.
# See "class env" defined later for the implementation.

INITIAL_ENV = {
  # Internal settings
  'EMIT_LL'     : '0',   # Produce an intermediate .ll file
  'USE_MC_ASM'  : '0',   # Use llvm-mc instead of nacl-as
  'MC_DIRECT'   : '0',   # Use MC direct object emission instead of
                         # producing an intermediate .s file
  'CLEANUP'     : '0',   # Clean up temporary files
  'SANDBOXED_ARCH': '',  # Use sandboxed toolchain for this arch.
                         # Current options are X8632 or X8664.

  # Command-line options
  'GCC_MODE'    : '',     # '' (default), '-E', '-c', or '-S'
  'EMIT_LLVM'   : '0',    # -emit-llvm
  'PIC'         : '0',    # -fPIC
  'STATIC'      : '1',    # -static was given (on by default for now)
  'NOSTDINC'    : '0',    # -nostdinc
  'NOSTDLIB'    : '0',    # -nostdlib
  'DIAGNOSTIC'  : '0',    # Diagnostic flag detected

  'INPUTS'      : '',    # Input files
  'OUTPUT'      : '',    # Output file
  'UNMATCHED'   : '',    # All unrecognized parameters

  # Special settings needed to tweak behavior
  'SKIP_OPT'    : '0',  # Don't run OPT. This is used in cases where
                        # OPT might break things.
  'PRELINKED'   : '0',  # Controls whether an input .bc file should be
                        # considered pre-linked or post-linked.

  'BASE_NACL'       : '', # Filled in later
  'BASE'            : '${BASE_NACL}/toolchain/linux_arm-untrusted',
  'BASE_ARM'        : '${BASE}/arm-none-linux-gnueabi',
  'BASE_ARM_INCLUDE': '${BASE_ARM}/arm-none-linux-gnueabi/include',

  'LOG_TO_FILE'          : '1',
  'LOG_FILENAME'         : '${BASE_NACL}/toolchain/hg-log/driver.log',
  'LOG_FILE_SIZE_LIMIT'  : '20971520', # 20 MB
  'LOG_PRETTY_PRINT'     : '1',

  'GOLD_PLUGIN_SO'  : '${BASE_ARM}/lib/libLLVMgold.so',
  'GOLD_PLUGIN_ARGS': '-plugin=${GOLD_PLUGIN_SO} -plugin-opt=emit-llvm',

  'ROOT_ARM'    : '${BASE}/libs-arm',
  'ROOT_X8632'  : '${BASE}/libs-x8632',
  'ROOT_X8664'  : '${BASE}/libs-x8664',
  'ROOT_BC'     : '${BASE}/libs-bitcode',

  'TRIPLE_ARM'  : 'armv7a-none-linux-gnueabi',
  'TRIPLE_X8632': 'i686-none-linux-gnu',
  'TRIPLE_X8664': 'x86_64-none-linux-gnu',

  'LLVM_GCC'    : '${BASE_ARM}/bin/arm-none-linux-gnueabi-gcc',
  'CC_FLAGS'    : '${OPT_LEVEL} -fuse-llvm-va-arg -Werror-portable-llvm ' +
                  '-nostdinc -DNACL_LINUX=1 -D__native_client__=1',
  'CC_STDINC'   :
    # NOTE: the two competing approaches here
    #       make the gcc driver "right" or
    #       put all the logic/knowloedge into this driver.
    #       Currently, we have a messy mixture.
    '-isystem ${BASE_ARM}/lib/gcc/arm-none-linux-gnueabi/4.2.1/include ' +
    '-isystem ' +
    '${BASE_ARM}/lib/gcc/arm-none-linux-gnueabi/4.2.1/install-tools/include ' +
    '-isystem ${BASE_ARM}/include/c++/4.2.1 ' +
    '-isystem ${BASE_ARM}/include/c++/4.2.1/arm-none-linux-gnueabi ' +
    '-isystem ${BASE_ARM_INCLUDE} ' +
    # NOTE: order important
    # '-isystem',
    # BASE + '/arm-newlib/arm-none-linux-gnueabi/usr/include/nacl/abi',
    '-isystem ${BASE}/arm-newlib/arm-none-linux-gnueabi/include',

  # Sandboxed tools
  'SEL_LDR_X8632' : '${BASE_NACL}/scons-out/opt-linux-x86-32/staging/sel_ldr',
  'SEL_LDR_X8664' : '${BASE_NACL}/scons-out/opt-linux-x86-64/staging/sel_ldr',

  'BASE_SB_X8632' : '${BASE}/tools-sb/x8632',
  'BASE_SB_X8664' : '${BASE}/tools-sb/x8664',

  'LLC_SB_X8632'  : '${SEL_LDR_X8632} -a -- ${BASE_SB_X8632}/bin/llc',
  'AS_SB_X8632'   : '${SEL_LDR_X8632} -a -- ${BASE_SB_X8632}/bin/as',
  'LD_SB_X8632'   : '${SEL_LDR_X8632} -a -- ${BASE_SB_X8632}/bin/ld',

  'LLC_SB_X8664'  : '${SEL_LDR_X8664} -a -- ${BASE_SB_X8664}/bin/llc',
  'AS_SB_X8664'   : '${SEL_LDR_X8664} -a -- ${BASE_SB_X8664}/bin/as',
  'LD_SB_X8664'   : '${SEL_LDR_X8664} -a -- ${BASE_SB_X8664}/bin/ld',

  'LD_SB_FLAGS'   : '-nostdlib -T ${BASE_SB_%arch%}/script/ld_script -static',

  'LLVM_MC'       : '${BASE_ARM}/bin/llvm-mc',
  'LLVM_AS'       : '${BASE_ARM}/bin/llvm-as',
  'LLVM_DIS'      : '${BASE_ARM}/bin/llvm-dis',
  'LLVM_LINK'     : '${BASE_ARM}/bin/llvm-link',

  'LLC'           : '${BASE_ARM}/bin/llc',

  'LLC_FLAGS' : '',
  'LLC_FLAGS_ARM' :
    # The following options might come in hand and are left here as comments:
    # TODO(robertm): describe their purpose
    #     '-soft-float -aeabi-calls -sfi-zero-mask',
    # NOTE: we need a fairly high fudge factor because of
    # some vfp instructions which only have a 9bit offset
    ('-march=arm -mcpu=cortex-a8 -mattr=-neon -mattr=+vfp2 -arm-reserve-r9 ' +
     '-sfi-disable-cp -arm_static_tls -asm-verbose=false ' +
     '-sfi-store -sfi-stack -sfi-branch -sfi-data ' +
     '-no-inline-jumptables'),
  'LLC_FLAGS_X8632' : '-march=x86 -mcpu=pentium4 -asm-verbose=false',
  'LLC_FLAGS_X8664' : '-march=x86-64 -mcpu=core2 -asm-verbose=false',

  'OPT'      : '${BASE_ARM}/bin/opt',
  'OPT_FLAGS': '-std-compile-opts -O3',
  'OPT_LEVEL': '',

  'OBJDUMP_ARM'   : '${BASE_ARM}/bin/arm-none-linux-gnueabi-objdump',
  'OBJDUMP_X8632' : '${BASE_ARM}/bin/arm-none-linux-gnueabi-objdump',
  'OBJDUMP_X8664' : '${BASE_ARM}/bin/arm-none-linux-gnueabi-objdump',

  'AS_ARM'        : '${BASE_ARM}/bin/arm-none-linux-gnueabi-as',
  'AS_X8632'      : '${BASE_NACL}/toolchain/linux_x86/bin/nacl-as',
  'AS_X8664'      : '${BASE_NACL}/toolchain/linux_x86/bin/nacl64-as',

  'AS_FLAGS_ARM'  : '-mfpu=vfp -march=armv7-a',
  'AS_FLAGS_X8632': '--32 --nacl-align 5 -n -march=pentium4 -mtune=i386',
  'AS_FLAGS_X8664': '--64 --nacl-align 5 -n -mtune=core2',

  'MC_FLAGS_ARM'   : '-assemble -filetype=obj -arch=arm -triple=armv7a-nacl',
  'MC_FLAGS_X8632' : '-assemble -filetype=obj -arch=i686 -triple=i686-nacl',
  'MC_FLAGS_X8664' : '-assemble -filetype=obj -arch=x86_64 -triple=x86_64-nacl',

  'LD'       : '${BASE_ARM}/bin/arm-none-linux-gnueabi-ld',
  'LD_FLAGS' : '--native-client -Ttext=0x00020000 -nostdlib -static',

  'BCLD'      : '${BASE_ARM}/bin/arm-none-linux-gnueabi-ld',
  'BCLD_FLAGS': '${GOLD_PLUGIN_ARGS}',


  'STDLIB_NATIVE_PREFIX': '${ROOT_%arch%}/crt1.o ${ROOT_%arch%}/crti.o ' +
                          '${ROOT_%arch%}/crtbegin.o',

  'STDLIB_NATIVE_SUFFIX': '${ROOT_%arch%}/libcrt_platform.a ' +
                          '${ROOT_%arch%}/crtend.o ${ROOT_%arch%}/crtn.o ' +
                          '-L${ROOT_%arch%} -lgcc_eh -lgcc',

  'STDLIB_BC_PREFIX': '${ROOT_BC}/nacl_startup.o',

  'STDLIB_BC_SUFFIX': '-L${ROOT_BC} -lc -lnacl -lstdc++ -lc -lnosys',

  # Build actual command lines below
  'RUN_OPT': '${OPT} ${OPT_FLAGS} "${input}" -f -o "${output}"',

  'RUN_LLC': '${LLC} ${LLC_FLAGS} ${LLC_FLAGS_%arch%} ' +
             '-mtriple=${TRIPLE_%arch%} ' +
             '-filetype=${filetype} ' +
             '"${input}" -o "${output}"',

  'RUN_LLVM_AS'  : '${LLVM_AS} "${input}" -o "${output}"',
  'RUN_LLVM_LINK': '${LLVM_LINK} ${inputs} -o "${output}"',

  'RUN_NACL_AS' : '${AS_%arch%} ${AS_FLAGS_%arch%} "${input}" -o "${output}"',
  'RUN_LLVM_MC' : '${LLVM_MC} ${MC_FLAGS_%arch%} "${input}" -o "${output}"',

  'RUN_GCC': '${LLVM_GCC} -emit-llvm ${mode} ${CC_FLAGS} ${CC_STDINC} ' +
             '-fuse-llvm-va-arg "${input}" -o "${output}"',

  'RUN_PP' : '${LLVM_GCC} -E ${CC_FLAGS} ${CC_STDINC} ' +
             '"${input}" -o "${output}"',

  'RUN_LLVM_DIS'    : '${LLVM_DIS} "${input}" -o "${output}"',
  'RUN_NATIVE_DIS'  : '${OBJDUMP_%arch%} -d "${input}"',

  # Multiple input actions
  'RUN_LD' : '${LD} ${LD_FLAGS} ${STDLIB_NATIVE_PREFIX} ${inputs} ' +
             '${STDLIB_NATIVE_SUFFIX} -o "${output}"',

  'RUN_BCLD': '${BCLD} ${BCLD_FLAGS} ' +
              '${STDLIB_NATIVE_PREFIX} ${STDLIB_BC_PREFIX} ${inputs} ' +
              '${STDLIB_BC_SUFFIX} ${STDLIB_NATIVE_SUFFIX} ' +
              '-o "${output}"'
}

######################################################################
# Patterns for command-line arguments
######################################################################
DriverPatterns = [
  ( '--pnacl-driver-verbose',          "Log.LOG_OUT.append(sys.stderr)"),
  ( '--driver=(.+)',                   "env.set('LLVM_GCC', $0)"),
  ( '--pnacl-driver-set-([^=]+)=(.*)', "env.set($0, $1)"),
  ( ('-arch','(.+)'),                  "env.set('ARCH', FixArch($0))"),
  ( '--pnacl-dis',                     "env.set('INCARNATION', 'dis')"),
  ( '--pnacl-as',                      "env.set('INCARNATION', 'as')"),
  ( '--pnacl-gcc',                     "env.set('INCARNATION', 'gcc')"),
  ( '--pnacl-bcld',                    "env.set('INCARNATION', 'bcld')"),
  ( '--pnacl-translate',               "env.set('INCARNATION', 'translate')"),
  ( '-emit-llvm',                      "env.set('EMIT_LLVM', '1')"),
  ( '--emit-llvm',                     "env.set('EMIT_LLVM', '1')"),
  ( '--pnacl-sb-arm',                  "env.set('SANDBOXED_ARCH', 'ARM')"),
  ( '--pnacl-sb-x86-32',               "env.set('SANDBOXED_ARCH', 'X8632')"),
  ( '--pnacl-sb-x86-64',               "env.set('SANDBOXED_ARCH', 'X8664')"),
]

GCCPatterns = [
  ( '-o(.+)',          "env.set('OUTPUT', $0)"),
  ( ('-o', '(.+)'),    "env.set('OUTPUT', $0)"),

  ( '-E',              "env.set('GCC_MODE', '-E')"),
  ( '-S',              "env.set('GCC_MODE', '-S')"),
  ( '-c',              "env.set('GCC_MODE', '-c')"),

  ( '-fPIC',           "env.set('PIC', '1')"),
  ( '-nostdinc',       "env.set('NOSTDINC', '1')"),
  ( '-nostdlib',       "env.set('NOSTDLIB', '1')"),

  ( '(.+\\.c)',        "env.append('INPUTS', $0)"),
  ( '(.+\\.cc)',       "env.append('INPUTS', $0)"),
  ( '(.+\\.cpp)',      "env.append('INPUTS', $0)"),
  ( '(.+\\.C)',        "env.append('INPUTS', $0)"),
  ( '(.+\\.s)',        "env.append('INPUTS', $0)"),
  ( '(.+\\.S)',        "env.append('INPUTS', $0)"),
  ( '(.+\\.asm)',      "env.append('INPUTS', $0)"),
  ( '(.+\\.bc)',       "env.append('INPUTS', $0)"),
  ( '(.+\\.o)',        "env.append('INPUTS', $0)"),
  ( '(.+\\.os)',       "env.append('INPUTS', $0)"),
  ( '(.+\\.ll)',       "env.append('INPUTS', $0)"),
  ( '(.+\\.nexe)',     "env.append('INPUTS', $0)"),
  ( '(.+\\.pexe)',     "env.append('INPUTS', $0)"),
  ( '(-l.+)',          "env.append('INPUTS', $0)"),

  ( '-O(.+)',          "env.set('OPT_LEVEL', '-O' + $0)"),

  ( '(-W.*)',          "env.append('CC_FLAGS', $0)"),
  ( '(-std=.*)',       "env.append('CC_FLAGS', $0)"),
  ( '(-B.*)',          "env.append('CC_FLAGS', $0)"),

  ( '(-D.*)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-f.*)',                 "env.append('CC_FLAGS', $0)"),
  ( ('-I','(.+)'),            "env.append('CC_FLAGS', '-I' + $0)"),
  ( '(-I.+)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-pedantic)',            "env.append('CC_FLAGS', $0)"),
  ( ('(-isystem)','(.+)'),    "env.append('CC_FLAGS', $0, $1)"),
  ( '-shared',                "env.set('STATIC', '0')"),
  ( '-static',                "env.set('STATIC', '1')"),
  ( '(-g.*)',                 "env.append('CC_FLAGS', $0)"),
  ( '(-xassembler-with-cpp)', "env.append('CC_FLAGS', $0)"),

  ( ('-L','(.+)'),            "env.append('BCLD_FLAGS', '-L' + $0)"),
  ( '(-L.+)',                 "env.append('BCLD_FLAGS', $0)"),
  ( '(-Wp,.*)',               "env.append('CC_FLAGS', $0)"),

  # Ignore these gcc flags
  ( '(-msse)',                ""),
  ( '(-march=armv7-a)',       ""),

  # Ignore these assembler flags
  ( '(-Qy)',                  ""),
  ( ('(--traditional-format)', '.*'), ""),
  ( '(-gstabs)',              ""),
  ( '(-gdwarf2)',             ""),
  ( '(--fatal-warnings)',     ""),

  # GCC diagnostic mode triggers
  ( '(-print-.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(--print.*)',            "env.set('DIAGNOSTIC', '1')"),
  ( '(-dumpspecs)',           "env.set('DIAGNOSTIC', '1')"),
  ( '(-v|--version|--v|-V)',  "env.set('DIAGNOSTIC', '1')"),
  ( '(-d.*)',                 "env.set('DIAGNOSTIC', '1')"),
]

CatchAllPattern = [
  ( '(.*)',                   "env.append('UNMATCHED', $0)"),
]

def FixArch(arch):
  archfix = { 'x86-32': 'X8632',
              'x86-64': 'X8664',
              'arm'   : 'ARM' }
  if arch not in archfix:
    Log.Fatal("Unrecognized arch '%s'!" % arch)
  return archfix[arch]

def PrepareFlags():
  if env.getbool('PIC'):
    env.append('LLC_FLAGS', '-relocation-model=pic')

  if env.getbool('NOSTDINC'):
    env.clear('CC_STDINC')

  if env.getbool('NOSTDLIB'):
    env.clear('STDLIB_NATIVE_PREFIX')
    env.clear('STDLIB_NATIVE_SUFFIX')
    env.clear('STDLIB_BC_PREFIX')
    env.clear('STDLIB_BC_SUFFIX')

  sbtc = env.get('SANDBOXED_ARCH')
  if sbtc != '':
    env.set('LLC', '${LLC_SB_%s}' % sbtc)
    env.set('AS', '${AS_SB_%s}' % sbtc)
    env.set('LD', '${LD_SB_%s}' % sbtc)
    env.set('LD_FLAGS', '${LD_SB_FLAGS}')

######################################################################
# Chain Map
######################################################################

# Order is very important here! These rules are scanned in order.
# The first rule seen that brings us "closer" to the output type is
# the one chosen.
#
# Closeness is determined by using ChainList. For example, 'll' is closer
# to 'bc' than 'c', because 'll' occurs after 'c' but before 'bc'.
#
# For example, if we are trying to compile cc -> bc and EMIT_LL is true,
# then the rule c -> ll will be selected first because it is the first rule
# on the list which brings us further along the chain towards bc. Next,
# the ll -> bc rule will be used to complete the transformation. This produces
# an intermediate ll file.
#
# However, if EMIT_LL is false, then the c -> ll rule is moved downward.
# As a result, the c -> bc rule will be selected first, and no
# intermediate .ll file will be produced.
def PrepareChainMap():
  ChainOrder = [
    'src',
    'pp',
    'll',
    'bc',
    'pexe',
    'S',
    's',
    'o',
    'nexe',
    'lib'
  ]

  ChainMapInit = [
    # Inputs -> Out   , Function, ExtraArgs
    ( 'src -> ll'     , 'RUN_GCC', {'mode': '-S'}),
    ( 'src -> bc'     , 'RUN_GCC', {'mode': '-c'}),
    ( 'src -> pp'     , 'RUN_PP', {}),
    ( 'll -> bc'      , 'RUN_LLVM_AS', {} ),
    ( 'bc|pexe -> o'  , 'RUN_LLC', { 'filetype': 'obj' }),
    ( 'bc|pexe -> s'  , 'RUN_LLC', { 'filetype': 'asm' }),
    ( 'S -> s'        , 'RUN_PP', {}),
    ( 's -> o'        , 'RUN_LLVM_MC', {}),
    ( 's -> o'        , 'RUN_NACL_AS', {}),
  ]

  # If we are told not to emit intermediate .ll files,
  # then push the 'src -> ll' rule to the bottom
  # so that the 'src -> bc' rule is used first instead.
  if not env.getbool('EMIT_LL'):
    assert(ChainMapInit[0][0] == 'src -> ll')
    ChainMapInit = ChainMapInit[1:] + ChainMapInit[0:1]

  # If MC direct object emission is disabled, then
  # delete the 'bc|pexe -> o' rule which causes it.
  if not env.getbool('MC_DIRECT'):
    priorlen = len(ChainMapInit)
    ChainMapInit = [r for r in ChainMapInit
                    if r[0] != 'bc|pexe -> o']
    assert(len(ChainMapInit) == priorlen - 1)

  # If MC ASM is disabled, remove the 's -> o' rule
  # that uses RUN_LLVM_MC. This forces us to choose the
  # rule that uses RUN_NACL_AS instead.
  if not env.getbool('USE_MC_ASM'):
    priorlen = len(ChainMapInit)
    ChainMapInit = [ rule for rule in ChainMapInit
                     if rule[1] != 'RUN_LLVM_MC' ]
    assert(len(ChainMapInit) == priorlen - 1)

  ChainMap.reset(ChainOrder, ChainMapInit)


######################################################################
# Main and Incarnations
######################################################################

def main(argv):
  env.reset()
  Log.reset()
  Log.Banner(argv)

  ParseArgs(argv[1:], DriverPatterns + GCCPatterns + CatchAllPattern)

  if env.getbool('DIAGNOSTIC'):
    # "configure", especially when run as part of a toolchain bootstrap
    # process, will invoke gcc with various diagnostic options and
    # parse the output. In these cases we do not alter the incoming
    # commandline. It is also important to not emit spurious messages.
    env.reset()
    ParseArgs(argv[1:], DriverPatterns + CatchAllPattern)
    RunWithLog("${LLVM_GCC} ${UNMATCHED}")
    return 0

  unmatched = env.get('UNMATCHED').strip()
  if len(unmatched) > 0:
    Log.Fatal('Unmatched parameters: ' + unmatched)

  PrepareFlags()
  PrepareChainMap()

  if argv[0].endswith('.py'):
    incarnation = "gcc"
    if env.has('INCARNATION'):
      incarnation = env.get('INCARNATION')
  else:
    incarnation = argv[0].split('-')[-1]

  if incarnation in ('sfigcc', 'sfig++', 'gcc', 'g++'):
    incarnation = 'gcc'

  func = globals().get('Incarnation_' + incarnation)
  if not func:
    Log.Fatal("Unknown incarnation: " + incarnation)

  return func()

def GetArch():
  if not env.has('ARCH'):
    Log.Fatal("Missing -arch!")
  arch = env.get('ARCH')
  return arch

def Incarnation_gcc():
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  emit_llvm = env.getbool('EMIT_LLVM')
  arch = None if emit_llvm else GetArch()
  gcc_mode = env.get('GCC_MODE')
  output_type_map = {
    ('-E', True) : 'pp',
    ('-E', False): 'pp',
    ('-c', True) : 'bc',
    ('-c', False): 'o',
    ('-S', True) : 'll',
    ('-S', False): 's',
    ('',   True) : 'pexe',
    ('',   False): 'nexe' }
  output_type = output_type_map[(gcc_mode, emit_llvm)]
  if output == '' and output_type in ('pexe', 'nexe'):
    output = 'a.out'
  elif output == '' and output_type == 'pp':
    output = '-'
  elif output == '' and len(inputs) == 1:
    output = DefaultOutputName(inputs[0], output_type)

  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_bcld():
  # TODO(pdox): Fix the optimization bug(s) that require this.
  # See: http://code.google.com/p/nativeclient/issues/detail?id=1225
  env.set('SKIP_OPT', '1')

  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  emit_llvm = env.getbool('EMIT_LLVM')
  output_type = 'pexe' if emit_llvm else 'nexe'
  arch = None if emit_llvm else GetArch()
  for i in inputs:
    if FileType(i) not in ('bc','o','lib'):
      Log.Fatal("Expecting only bitcode files for bcld invocation")
  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_translate():
  # TODO(pdox): Fix this by arranging for combined bitcode file
  #             to have extension .pexe instead of .bc or .o
  env.set('PRELINKED', '1')
  env.set('SKIP_OPT', '1')
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  output_type = 'nexe'
  arch = GetArch()
  assert(len(inputs) == 1)
  if output == '':
    output = DefaultOutputName(inputs[0], output_type)
  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_as_x86_32():
  env.set('ARCH', 'X8632')
  Incarnation_as()

def Incarnation_as_x86_64():
  env.set('ARCH', 'X8664')
  Incarnation_as()

def Incarnation_as():
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  emit_llvm = env.getbool('EMIT_LLVM')
  output_type = 'bc' if emit_llvm else 'o'
  arch = None if emit_llvm else GetArch()

  assert(len(inputs) == 1)
  if output == '':
    output = DefaultOutputName(inputs[0], output_type)
  Compile(arch, inputs, output, output_type)
  return 0

def Incarnation_bclink():
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  if output == '':
    Log.Fatal("Please specify output name")
  RunWithEnv('RUN_LLVM_LINK',
             inputs=shell.join(inputs),
             output=output)
  return 0

def Incarnation_bcopt():
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  if len(inputs) != 1:
    Log.Fatal("Expected exactly one input")
  if output == '':
    Log.Fatal("Please specify output name")
  OptimizeBC(inputs[0], output)
  return 0

def Incarnation_dis():
  inputs = shell.split(env.get('INPUTS'))
  output = env.get('OUTPUT')
  if len(inputs) != 1:
    Log.Fatal('Expecting exactly one input file')
  intype = FileType(inputs[0])
  if output == '':
    output = '-'
  if intype in ('o', 'nexe'):
    arch = GetArch()
    RunWithEnv('RUN_NATIVE_DIS', input=inputs[0], arch=arch)
  elif intype in ('pexe', 'bc'):
    RunWithEnv('RUN_LLVM_DIS', input=inputs[0], output=output)
  else:
    Log.Fatal("Unknown file type")
  return 0

def Incarnation_illegal():
  Log.Fatal('ILLEGAL COMMAND: ' + StringifyCommand(sys.argv))

def Incarnation_nop():
  Log.Info('IGNORING: ' + StringifyCommand(sys.argv))
  NiceExit(0)

def NiceExit(code):
  sys.exit(code)

######################################################################
# Compiling / Linking Engine
######################################################################

def CompileOne(arch, outtype, infile, output = None):
  """Compile a single file all the way to type 'outtype'"""
  assert((output is None) or (output != ''))
  intype = FileType(infile)
  curfile = infile

  path = ChainMap.GetPath(intype, outtype)
  if path is None:
    Log.Fatal("Unable to find a compilation path!")
  for (cur_type, next_type, cmd, extra) in path:
    if next_type != outtype or output is None:
      # We are creating a temporary file
      nextfile = TempNameForInput(infile, next_type)
    else:
      # We are emitting the final output
      nextfile = output

    RunWithEnv(cmd, arch=arch, input=curfile, output=nextfile, **extra)
    # <PIC HACK>
    if next_type == 's' and env.getbool('PIC') and arch == "X8632":
      X8632TLSHack(nextfile)
    # </PIC HACK>
    curfile = nextfile

  return curfile


def Compile(arch, inputs, output, output_type):
  """Compile and/or link one or more files all the way to output_type"""

  if len(inputs) == 0:
    Log.Fatal("No input files")

  # If there are multiple input files and no linking is being done,
  # then there are multiple outputs. Handle this by recursively
  # calling Compile for each one.
  if len(inputs) > 1 and output_type not in ('nexe', 'pexe'):
    if output != '':
      Log.Fatal("Cannot have -o with -c, -S, or -E and multiple inputs")

    for infile in inputs:
      output = DefaultOutputName(infile, output_type)
      Compile(arch, [infile], output, output_type)
    return

  if output == '':
    Log.Fatal("Output file name must be specified")

  InitTempNames(inputs, output)

  # Handle single file, non-linking case
  if output_type not in ('nexe', 'pexe'):
    assert(len(inputs) == 1)
    CompileOne(arch, output_type, inputs[0], output)
    return 0

  # Compile all source files (c/c++/ll) to .bc
  for i in xrange(0, len(inputs)):
    intype = FileType(inputs[i])
    if ChainMap.PathExists(intype, 'bc'):
      inputs[i] = CompileOne(arch, 'bc', inputs[i])

  # Compile all .s/.S/.pexe to .o
  if output_type == 'nexe':
    for i in xrange(0, len(inputs)):
      intype = FileType(inputs[i])
      if intype in ('s','S','pexe'):
        inputs[i] = CompileOne(arch, 'o', inputs[i])

  # We should only be left with .bc and .o and libraries
  for i in xrange(0, len(inputs)):
    intype = FileType(inputs[i])
    assert(intype in ('bc','pexe','o','lib'))

  if output_type == 'nexe':
    LinkAll(arch, inputs, output)
  elif output_type == 'pexe':
    LinkBC(inputs, output)
  else:
    Log.Fatal("Unexpected case")

  ClearTemps()

def LinkAll(arch, inputs, output):
  if not env.getbool('PRELINKED'):
    combined_bitcode = LinkBC(inputs)
  else:
    assert(len(inputs) == 1)
    combined_bitcode = inputs[0]

  if combined_bitcode:
    # Translate it to an object
    combined_obj = CompileOne(arch, 'o', combined_bitcode)

    # Substitute this object for the bitcode objects in the
    # native linker input
    inputs = [f for f in inputs if FileType(f) != 'bc']
    inputs = [combined_obj] + inputs

  # Finally, link to nexe
  LinkNative(arch, inputs, output)

# Input: a bunch of bc/o/lib input files
# Output: a combined & optimized bitcode file
def LinkBC(inputs, output = None):
  assert(output is None or output != '')

  # Make sure our inputs are only .bc, .o, and -l
  NeedsLinking = False
  for i in xrange(0, len(inputs)):
    intype = FileType(inputs[i])
    assert(intype in ('bc', 'pexe', 'o', 'lib'))
    if intype == 'bc':
      NeedsLinking = True

  if not NeedsLinking:
    return None

  SkipOpt = env.getbool('SKIP_OPT')
  if SkipOpt:
    if output is None:
      output = TempNameForOutput('bc')
    bcld_output = output
  else:
    if output is None:
      output = TempNameForOutput('opt.bc')
    bcld_output = TempNameForOutput('bc')

  # Produce combined bitcode file
  # There is an architecture bias here. To link the bitcode together,
  # we need to specify the native libraries to be used in the final linking,
  # just so the linker can resolve the symbols. We'd like to eliminate
  # this somehow.
  RunWithEnv('RUN_BCLD', arch = 'ARM',
             inputs = shell.join(inputs),
             output = bcld_output)

  if not SkipOpt:
    OptimizeBC(bcld_output, output)

  return output

def OptimizeBC(input, output):
  # Optimize combined bitcode file
  RunWithEnv('RUN_OPT',
             input = input,
             output = output)

# Final native linking step
def LinkNative(arch, inputs, output):
  inputs = filter(lambda x: not FileType(x) == 'lib', inputs)
  if not env.getbool('STATIC'):
    ld_flags = env.get('LD_FLAGS')
    ld_flags = ld_flags.replace('-static', '-shared')
    ld_flags = re.sub(r"-Ttext=0x[0-9a-fA-F]+ ", "", ld_flags)
    env.set('LD_FLAGS', ld_flags)

  RunWithEnv('RUN_LD', arch = arch,
             inputs = shell.join(inputs),
             output = output)
  return

######################################################################
# Chain Logic
######################################################################

class ChainMap:
  Order = []
  Rules = []
  OrderLookup = dict()

  @classmethod
  def reset(cls, ChainOrder, ChainMapInit):
    cls.Order = ChainOrder
    cls.OrderLookup = dict([ (b,a) for (a,b) in enumerate(cls.Order) ])

    # Convert ChainMapInit into Rules
    cls.Rules = []
    for (pat, cmd, extra) in ChainMapInit:
      pat = pat.replace(' ','')
      inputs, output = pat.split('->')
      inputs = inputs.split('|')
      for i in inputs:
        cls.Rules.append((i, output, cmd, extra))

  # Find the next compilation step which will bring
  # intype closer to being outtype
  # If none exists, returns None
  @classmethod
  def NextStep(cls, intype, outtype):
    innum = cls.OrderLookup[intype]
    outnum = cls.OrderLookup[outtype]

    if innum >= outnum:
      return None
    for (i, o, cmd, extra) in cls.Rules:
      if i != intype:
        continue
      if cls.OrderLookup[o] > outnum:
        continue
      return (i, o, cmd, extra)
    return None

  # Returns a list of tuples of the form:
  # (in_type, out_type, cmd, extra)
  # Which describes what actions to perform to move from
  # input_type to output_type
  @classmethod
  def GetPath(cls, input_type, output_type):
    cur_type = input_type
    result = []
    while cur_type != output_type:
      step = cls.NextStep(cur_type, output_type)
      if step is None:
        return None
      result.append(step)
      cur_type = step[1]
    return result

  @classmethod
  def PathExists(cls, input_type, output_type):
    path = cls.GetPath(input_type, output_type)
    return path is not None

######################################################################
# Environment
######################################################################

class env:
  data = []
  stack = []

  @classmethod
  def reset(cls):
    cls.data = dict(INITIAL_ENV)
    cls.set('BASE_NACL', FindBaseDir())

  @classmethod
  def dump(cls):
    for (k,v) in cls.data.iteritems():
      print '%s == %s' % (k,v)

  @classmethod
  def push(cls):
    cls.stack.append(cls.data)
    cls.data = dict(cls.data) # Make a copy

  @classmethod
  def pop(cls):
    cls.data = cls.stack.pop()

  @classmethod
  def has(cls, varname):
    return varname in cls.data

  # Retrieve and evaluate a variable from the environment
  @classmethod
  def get(cls, varname):
    return cls.eval(cls.data[varname])

  @classmethod
  def getbool(cls, varname):
    return bool(int(cls.get(varname)))

  # Set a variable in the environment
  @classmethod
  def set(cls, varname, val):
    cls.data[varname] = val

  @classmethod
  def clear(cls, varname):
    cls.data[varname] = ''

  # Append one or more terms to a variable in the environment
  @classmethod
  def append(cls, varname, *vals):
    x = ''
    for v in vals:
      x += ' ' + shell.escape(v)

    if not varname in cls.data:
      cls.data[varname] = ''
    else:
      cls.data[varname] += ' '
    cls.data[varname] += x.strip()

  # Evaluate an expression in the environment
  # Returns a string
  @classmethod
  def eval(cls, expr):
    lpos = expr.find('${')
    if lpos == -1:
      return expr
    rpos = expr.find('}', lpos)
    if rpos == -1:
      print 'Unterminated ${ starting at: ' + expr[lpos:]
      NiceExit(0)
    varname = expr[lpos+2:rpos]
    if '%' in varname:
      # This currently assumes only one '%' sub-expression
      sublpos = varname.find('%')
      subrpos = varname.rfind('%')
      assert(sublpos >= 0 and subrpos >= 0)
      sub = cls.eval('${' + varname[sublpos+1:subrpos] + '}')
      varname = varname[0:sublpos] + sub + varname[subrpos+1:]
    if varname not in cls.data:
      print 'Undefined variable name ' + varname
      NiceExit(1)
    return cls.eval(expr[0:lpos] + cls.data[varname] + expr[rpos+1:])

# Run a command with extra environment settings
def RunWithEnv(cmd, **kwargs):
  env.push()

  for (k,v) in kwargs.iteritems():
    env.set(k, v)

  RunWithLog('${%s}' % cmd)
  env.pop()

# Crawl backwards, starting from the directory containing this script,
# until we find the native_client/ directory.
def FindBaseDir():
  Depth = 0
  cur = os.path.abspath(sys.argv[0])
  while os.path.basename(cur) != 'native_client' and \
        Depth < 16:
    cur = os.path.dirname(cur)
    Depth += 1

  if os.path.basename(cur) != "native_client":
    print "Unable to find native_client directory!"
    sys.exit(1)

  return cur


######################################################################
# Argument Parsing
######################################################################

def MatchOne(argv, i, patternlist):
  """Find a pattern which matches argv starting at position i"""
  for (regex, action) in patternlist:
    if isinstance(regex, str):
      regex = [regex]
    j = 0
    matches = []
    for r in regex:
      if i+j < len(argv):
        match = re.compile('^'+r+'$').match(argv[i+j])
      else:
        match = None
      matches.append(match)
      j += 1
    if None in matches:
      continue
    groups = [ list(m.groups()) for m in matches ]
    groups = reduce(lambda x,y: x+y, groups, [])
    return (len(regex), action, groups)
  return (0, '', [])

def ParseArgs(argv, patternlist):
  """Parse argv using the patterns in patternlist"""

  i = 0
  while i < len(argv):
    num_matched, action, groups = MatchOne(argv, i, patternlist)
    if num_matched == 0:
      Log.Fatal('Unrecognized argument: ' + argv[i])
    for g in xrange(0, len(groups)):
      action = action.replace('$%d' % g, 'groups[%d]' % g)
    try:
      exec(action)
    except Exception:
      Log.Fatal("ParseArgs action failed: %s" % action)
    i += num_matched
  return

######################################################################
# File Naming Logic
######################################################################

def FileType(filename):
  if filename.startswith('-l'):
    return 'lib'

  ext = filename.split('.')[-1]

  # File Extension -> Type string
  ExtensionMap = {
    'c'   : 'src',
    'cc'  : 'src',
    'cpp' : 'src',
    'C'   : 'src',
    'll'  : 'll',
    'bc'  : 'bc',
    'po'  : 'bc',   # .po = "Portable object file"
    'pexe': 'pexe', # .pexe = "Portable executable"
    'asm' : 'S',
    'S'   : 'S',
    's'   : 's',
    'o'   : 'o',
    'os'  : 'o',
    'nexe': 'nexe'
  }
  if ext not in ExtensionMap:
    Log.Fatal('Unknown file extension: %s' % filename)

  # Ugly hack. Why do we name bitcode files .o ?
  if ext == 'o' or ext == 'os':
    try:
      fp = open(filename, 'rb')
    except Exception:
      print "Failed to open input file " + filename
      NiceExit(1)
    header = fp.read(2)
    fp.close()
    if header == 'BC':
      ext = 'bc'

  return ExtensionMap[ext]

def RemoveExtension(filename):
  if filename.endswith('.opt.bc'):
    return filename[0:-len('.opt.bc')]

  name, ext = os.path.splitext(filename)
  if ext == '':
    Log.Fatal('File has no extension: ' + filename)
  return name

def DefaultOutputName(filename, outtype):
  assert(outtype in ('pp','ll','bc','pexe','s','S','o','nexe','dis'))
  base = os.path.basename(filename)
  base = RemoveExtension(base)

  if outtype in ('pp','dis'): return '-'; # stdout
  if outtype in ('bc'): return base + '.o'
  return base + '.' + outtype

def PathSplit(f):
  paths = []
  cur = f
  while True:
    cur, piece = os.path.split(cur)
    if piece == '':
      break
    paths.append(piece)
  paths.reverse()
  return paths

# Generate a unique identifier for each input file.
# Start with the basename, and if that is not unique enough,
# add parent directories. Rinse, repeat.
TempBase = None
TempMap = None
TempList = []
def InitTempNames(inputs, output):
  global TempMap, TempBase
  inputs = [ os.path.abspath(i) for i in inputs ]
  output = os.path.abspath(output)

  TempMap = dict()
  TempBase = output

  if len(inputs) == 1:
    # There's only one input file, don't bother adding the source name.
    TempMap[inputs[0]] = output
    return

  # Build the initial mapping
  TempMap = dict()
  for f in inputs:
    path = PathSplit(f)
    TempMap[f] = [1, path]

  while True:
    # Find conflicts
    ConflictMap = dict()
    Conflicts = set()
    for (f, [n, path]) in TempMap.iteritems():
      candidate = output + '---' + '_'.join(path[-n:]) + '---'
      if candidate in ConflictMap:
        Conflicts.add(ConflictMap[candidate])
        Conflicts.add(f)
      else:
        ConflictMap[candidate] = f

    if len(Conflicts) == 0:
      break

    # Resolve conflicts
    for f in Conflicts:
      n = TempMap[f][0]
      if n+1 > len(TempMap[f][1]):
        Log.Fatal('Unable to resolve naming conflicts')
      TempMap[f][0] = n+1

  # Clean up the map
  NewMap = dict()
  for (f, [n, path]) in TempMap.iteritems():
    candidate = output + '---' + '_'.join(path[-n:]) + '---'
    NewMap[f] = candidate
  TempMap = NewMap
  return

def TempNameForOutput(imtype):
  global TempList, TempBase

  temp = TempBase + '.' + imtype
  TempList.append(temp)
  return temp

def TempNameForInput(input, imtype):
  global TempList
  fullpath = os.path.abspath(input)
  # If input is already a temporary name, just change the extension
  if fullpath.startswith(TempBase):
    temp = TempBase + '.' + imtype
  else:
    # Source file
    temp = TempMap[fullpath] + '.' + imtype

  TempList.append(temp)
  return temp

def ClearTemps():
  global TempList
  if env.getbool('CLEANUP'):
    for f in TempList:
      os.remove(f)

######################################################################
# Shell Utilities
######################################################################

class shell:
  @staticmethod
  def split(s):
    """Split a shell-style string up into a list of distinct arguments.
    For example: split('cmd -arg1 -arg2="a b c"')
    Returns ['cmd', '-arg1', '-arg2=a b c']
    """

    out = []
    inspace = True
    inquote = False
    buf = ''

    i = 0
    while i < len(s):
      if s[i] == '"':
        inspace = False
        inquote = not inquote
      elif s[i] == ' ' and not inquote:
        if not inspace:
          out.append(buf)
          buf = ''
        inspace = True
      elif s[i] == '\\':
        if not i+1 < len(s):
          Log.Fatal("Unterminated \\ escape sequence")
        inspace = False
        i += 1
        buf += s[i]
      else:
        inspace = False
        buf += s[i]
      i += 1
    if inquote:
      Log.Fatal("Unterminated quote")
    if not inspace:
      out.append(buf)
    return out

  @staticmethod
  def join(args):
    """Turn a list into a shell-style string For example:
       shell.join([ 'a', 'b', 'c d e' ]) = 'a b "c d e"'
    """
    if isinstance(args, str):
      return args

    out = ''
    for a in args:
      out += shell.escape(a) + ' '

    return out[0:-1]

  @staticmethod
  def escape(s):
    """Shell-escape special characters in a string
       Surround with quotes if necessary
    """
    s = s.replace('\\', '\\\\')
    s = s.replace('"', '\\"')
    if ' ' in s:
      s = '"' + s + '"'
    return s

######################################################################
# Logging
######################################################################

class Log:
  # Lists of streams
  prefix = ''
  LOG_OUT = []
  ERROR_OUT = [sys.stderr]

  @classmethod
  def reset(cls):
    cls.prefix = '%d: ' % os.getpid()
    if env.getbool('LOG_TO_FILE'):
      filename = env.get('LOG_FILENAME')
      sizelimit = int(env.get('LOG_FILE_SIZE_LIMIT'))
      cls.AddFile(filename, sizelimit)

  @classmethod
  def AddFile(cls, filename, sizelimit):
    file_too_big = os.path.isfile(filename) and \
                   os.path.getsize(filename) > sizelimit
    try:
        fp = open(filename, 'w' if file_too_big else 'a')
    except Exception:
        return
    cls.LOG_OUT.append(fp)

  @classmethod
  def Banner(cls, argv):
    cls.Info('PNaCl Driver Invoked With:\n' + StringifyCommand(argv))
    cls.Info('-'*60)

  @classmethod
  def Info(cls, m):
    cls.LogPrint(m)

  @classmethod
  def Fatal(cls, m, ret=-1):
    m = 'FATAL: ' + m
    cls.LogPrint(m)
    cls.ErrorPrint(m)
    NiceExit(ret)

  @classmethod
  def LogPrint(cls, m):
    for o in cls.LOG_OUT:
      print >> o, m

  @classmethod
  def ErrorPrint(cls, m):
    for o in cls.ERROR_OUT:
      print >> o, m

def StringifyCommand(a):
  if env.getbool('LOG_PRETTY_PRINT'):
    return PrettyStringify(a)
  else:
    return SimpleStringify(a)

def SimpleStringify(args):
  return " ".join(args)

def PrettyStringify(args):
  ret = ''
  grouping = 0
  for a in args:
    if grouping == 0 and len(ret) > 0:
      ret += " \\\n    "
    elif grouping > 0:
      ret += " "
    if grouping == 0:
      grouping = 1
      if a.startswith('-') and len(a) == 2:
        grouping = 2
    ret += a
    grouping -= 1
  return ret

def RunWithLog(args):
  "Run the commandline give by the list args system()-style"

  if isinstance(args, str):
    args = shell.split(env.eval(args))

  Log.Info('\n' + StringifyCommand(args))
  try:
    p = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE )
    buf_stdout, buf_stderr = p.communicate()
    ret = p.returncode
  except Exception, e:
    buf_stdout = ''
    buf_stderr = ''
    Log.Fatal('failed (%s) to run: ' % str(e) + StringifyCommand(args))

  sys.stdout.write(buf_stdout)
  sys.stderr.write(buf_stderr)

  if ret:
    Log.Fatal('failed command: ' + StringifyCommand(args) + '\n' +
              'stdout        :\n' + buf_stdout + '\n' +
              'stderr        :\n' + buf_stderr + '\n', ret)

######################################################################
# TLS Hack
######################################################################

def X8632TLSHack(asm_file):
  """ Work around http://code.google.com/p/nativeclient/issues/detail?id=237
      Rewrite the following TLS General Dynamic sequence generated by LLVM:
        leal  symbol@TLSGD(,%ebx),%eax;  call ___tls_get_addr@PTOFF

      into the TLS Initial Exec version:
        movl  %gs:0, %eax;  addl symbol@gotntpoff(%ebx), %eax

      At this point we know we are generating an executable, and Gold will do
      this optimization in this case -- see gold/i386.cc:optimize_tls_reloc

      TODO(jvoung) remove this hack once we can control bundling better.
  """
  Log.Info('Doing X86-32 TLS Hack')
  # Group \1 is prefix (typically whitespace) and \2 is the symbol.
  gd_pat = '\\(.*\\)leal\\s*\\(.*\\)@TLSGD.*call.*'
  gd_pat = shell.escape(gd_pat)
  # Note: %ebx and %eax are always the registers used.
  ie_pat = '\\1movl %gs:0,%eax;  addl \\2@gotntpoff(%ebx), %eax # TLS Hack'
  ie_pat = shell.escape(ie_pat)
  RunWithLog('sed --in-place -e s/%s/%s/ "%s"' % (gd_pat, ie_pat, asm_file))


######################################################################
# Invocation
######################################################################

if __name__ == "__main__":
  NiceExit(main(sys.argv))
