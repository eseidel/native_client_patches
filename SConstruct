# -*- python -*-
# Copyright 2008 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import atexit
import glob
import os
import stat
import subprocess
import sys
sys.path.append("./common")

from SCons.Errors import UserError
from SCons.Script import GetBuildFailures

import SCons.Warnings
SCons.Warnings.warningAsException()

# NOTE: Underlay for  src/third_party_mod/gtest
# TODO: try to eliminate this hack
Dir('src/third_party_mod/gtest').addRepository(
    Dir('#/../testing/gtest'))
Dir('tests/ppapi_tests').addRepository(Dir('#/../ppapi/tests'))

# ----------------------------------------------------------
# REPORT
# ----------------------------------------------------------
def PrintFinalReport():
  """This function is run just before scons exits and dumps various reports.
  """
  # Note, these global declarations are not strictly necessary
  global pre_base_env
  global CMD_COUNTER
  global ENV_COUNTER

  if pre_base_env.Bit('target_stats'):
    print
    print '*' * 70
    print 'COMMAND EXECUTION REPORT'
    print '*' * 70
    for k in sorted(CMD_COUNTER.keys()):
      print "%4d %s" % (CMD_COUNTER[k], k)

    print
    print '*' * 70
    print 'ENVIRONMENT USAGE REPORT'
    print '*' * 70
    for k in sorted(ENV_COUNTER.keys()):
      print "%4d  %s" % (ENV_COUNTER[k], k)

  failures = GetBuildFailures()
  if not failures:
    return

  print
  print '*' * 70
  print 'ERROR REPORT: %d failures' % len(failures)
  print '*' * 70
  print
  for f in failures:
    print "%s failed: %s\n" % (f.node, f.errstr)

atexit.register(PrintFinalReport)


def VerboseConfigInfo(env):
  "Should we print verbose config information useful for bug reports"
  if '--help' in sys.argv: return False
  if env.Bit('prebuilt') or env.Bit('built_elsewhere'): return False
  return env.Bit('sysinfo')

# ----------------------------------------------------------
# SANITY CHECKS
# ----------------------------------------------------------

# NOTE BitFromArgument(...) implicitly defines additional ACCEPTABLE_ARGUMENTS.
ACCEPTABLE_ARGUMENTS = set([
    # TODO: add comments what these mean
    # TODO: check which ones are obsolete
    ####  ASCII SORTED ####
    # Use a destination directory other than the default "scons-out".
    'DESTINATION_ROOT',
    'DOXYGEN',
    'MODE',
    'SILENT',
    # set build platform
    'buildplatform',
    # used for browser_tests
    'chrome_browser_path',
    # where should we store extrasdk libraries
    'extra_sdk_lib_destination',
    # where should we store extrasdk headers
    'extra_sdk_include_destination',
    # force emulator use by tests
    'force_emulator',
    # force sel_ldr use by tests
    'force_sel_ldr',
    # colon-separated list of compiler flags, e.g. "-g:-Wall".
    'nacl_ccflags',
    # colon-separated list of linker flags, e.g. "-lfoo:-Wl,-u,bar".
    'nacl_linkflags',
    # colon-separated list of pnacl bcld flags, e.g. "-lfoo:-Wl,-u,bar".
    # Not using nacl_linkflags since that gets clobbered in some tests.
    'pnacl_bcldflags',
    'naclsdk_mode',
    # set both targetplatform and buildplatform
    'platform',
    # Run tests under this tool (e.g. valgrind, tsan, strace, etc).
    # If the tool has options, pass them after comma: 'tool,--opt1,--opt2'.
    # NB: no way to use tools the names or the args of
    # which contains a comma.
    'run_under',
    # More args for the tool.
    'run_under_extra_args',
    # Multiply timeout values by this number.
    'scale_timeout',
    # enable use of SDL
    'sdl',
    # set target platform
    'targetplatform',
    # activates buildbot-specific presets
    'buildbot',
  ])


# Overly general to provide compatibility with existing build bots, etc.
# In the future it might be worth restricting the values that are accepted.
_TRUE_STRINGS = set(['1', 'true', 'yes'])
_FALSE_STRINGS = set(['0', 'false', 'no'])


# Converts a string representing a Boolean value, of some sort, into an actual
# Boolean value. Python's built in type coercion does not work because
# bool('False') == True
def StringValueToBoolean(value):
  # ExpandArguments may stick non-string values in ARGUMENTS. Be accommodating.
  if isinstance(value, bool):
    return value

  if not isinstance(value, basestring):
    raise Exception("Expecting a string but got a %s" % repr(type(value)))

  if value.lower() in _TRUE_STRINGS:
    return True
  elif value.lower() in _FALSE_STRINGS:
    return False
  else:
    raise Exception("Cannot convert '%s' to a Boolean value" % value)


def GetBinaryArgumentValue(arg_name, default):
  if not isinstance(default, bool):
    raise Exception("Default value for '%s' must be a Boolean" % arg_name)
  if arg_name not in ARGUMENTS:
    return default
  return StringValueToBoolean(ARGUMENTS[arg_name])


# name is the name of the bit
# arg_name is the name of the command-line argument, if it differs from the bit
def BitFromArgument(env, name, default, desc, arg_name=None):
  # In most cases the bit name matches the argument name
  if arg_name is None:
    arg_name = name

  DeclareBit(name, desc)
  assert arg_name not in ACCEPTABLE_ARGUMENTS, arg_name
  ACCEPTABLE_ARGUMENTS.add(arg_name)

  if GetBinaryArgumentValue(arg_name, default):
    env.SetBits(name)
  else:
    env.ClearBits(name)


# SetUpArgumentBits declares binary command-line arguments and converts them to
# bits. For example, one of the existing declarations would result in the
# argument "bitcode=1" causing env.Bit('bitcode') to evaluate to true.
# NOTE Command-line arguments are a SCons-ism that is separate from
# command-line options.  Options are prefixed by "-" or "--" whereas arguments
# are not.  The function SetBitFromOption can be used for options.
# NOTE This function must be called before the bits are used
# NOTE This function must be called after all modifications of ARGUMENTS have
# been performed. See: ExpandArguments
def SetUpArgumentBits(env):
  BitFromArgument(env, 'bitcode', default=False,
    desc="We are building bitcode")

  BitFromArgument(env, 'built_elsewhere', default=False,
    desc="The programs have already been built by another system")

  BitFromArgument(env, 'nacl_pic', default=False,
    desc="generate position indepent code for (P)NaCl modules")

  # Defaults on when --verbose is specified.
  # --verbose sets 'brief_comstr' to False, so this looks a little strange
  BitFromArgument(env, 'target_stats', default=not GetOption('brief_comstr'),
    desc="Collect and display information about which commands are executed "
      "during the build process")

  BitFromArgument(env, 'werror', default=True,
    desc="Treat warnings as errors (-Werror)")

  BitFromArgument(env, 'disable_nosys_linker_warnings', default=False,
    desc="Disable warning mechanism in src/untrusted/nosys/warning.h")

  BitFromArgument(env, 'naclsdk_validate', default=True,
    desc="Verify the presence of the SDK")

  BitFromArgument(env, 'nocpp', default=False,
    desc="Skip the compilation of C++ code")

  BitFromArgument(env, 'running_on_valgrind', default=False,
    desc="Compile and test using valgrind")

  BitFromArgument(env, 'build_vim', default=False,
    desc="Build vim")

  BitFromArgument(env, 'build_av_apps', default=True,
    desc="Build multi media apps")

  # This argument allows -lcrypt to be disabled, which
  # makes it easier to build x86-32 NaCl on x86-64 Ubuntu Linux,
  # where there is no -dev package for the 32-bit libcrypto
  BitFromArgument(env, 'use_libcrypto', default=True,
    desc="Use libcrypto")

  BitFromArgument(env, 'pp', default=False,
    desc="Enable pretty printing")

  BitFromArgument(env, 'dangerous_debug_disable_inner_sandbox',
    default=False, desc="Make sel_ldr less strict")

  # By default SCons does not use the system's environment variables when
  # executing commands, to help isolate the build process.
  BitFromArgument(env, 'use_environ', arg_name='USE_ENVIRON',
    default=False, desc="Expose existing environment variables to the build")

  # Defaults on when --verbose is specified
  # --verbose sets 'brief_comstr' to False, so this looks a little strange
  BitFromArgument(env, 'sysinfo', default=not GetOption('brief_comstr'),
    desc="Print verbose system information")


def CheckArguments():
  for key in ARGUMENTS:
    if key not in ACCEPTABLE_ARGUMENTS:
      print 'ERROR'
      print 'ERROR bad argument: %s' % key
      print 'ERROR'
      sys.exit(-1)


# Sets a command line argument. Dies if an argument with this name is already
# defined.
def SetArgument(key, value):
  print '    %s=%s' % (key, str(value))
  if key in ARGUMENTS:
    print 'ERROR: %s redefined' % (key, )
    sys.exit(-1)
  else:
    ARGUMENTS[key] = value

# Expands "macro" command line arguments.
def ExpandArguments():
  if ARGUMENTS.get('buildbot') == 'memcheck':
    print 'buildbot=memcheck expands to the following arguments:'
    SetArgument('run_under',
                'src/third_party/valgrind/memcheck.sh,' +
                '--error-exitcode=1')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot') == 'tsan':
    print 'buildbot=tsan expands to the following arguments:'
    SetArgument('run_under',
                'src/third_party/valgrind/tsan.sh,' +
                '--nacl-untrusted,--error-exitcode=1')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot') == 'tsan-trusted':
    print 'buildbot=tsan-trusted expands to the following arguments:'
    SetArgument('run_under',
                'src/third_party/valgrind/tsan.sh,' +
                '--error-exitcode=1')
    SetArgument('scale_timeout', 20)
    SetArgument('running_on_valgrind', True)
  elif ARGUMENTS.get('buildbot'):
    print 'ERROR: unexpected argument buildbot="%s"' % (
        ARGUMENTS.get('buildbot'), )
    sys.exit(-1)

#-----------------------------------------------------------
ExpandArguments()

# ----------------------------------------------------------
environment_list = []

# ----------------------------------------------------------
# Base environment for both nacl and non-nacl variants.
kwargs = {}
if ARGUMENTS.get('DESTINATION_ROOT') is not None:
  kwargs['DESTINATION_ROOT'] = ARGUMENTS.get('DESTINATION_ROOT')
pre_base_env = Environment(
    tools = ['component_setup'],
    # SOURCE_ROOT is one leave above the native_client directory.
    SOURCE_ROOT = Dir('#/..').abspath,
    # Publish dlls as final products (to staging).
    COMPONENT_LIBRARY_PUBLISH = True,

    # Use workaround in special scons version.
    LIBS_STRICT = True,
    LIBS_DO_SUBST = True,

    # Select where to find coverage tools.
    COVERAGE_MCOV = '../third_party/lcov/bin/mcov',
    COVERAGE_GENHTML = '../third_party/lcov/bin/genhtml',
    **kwargs
)

# This function should be called ASAP after the environment is created, but
# after ExpandArguments.
SetUpArgumentBits(pre_base_env)

# Scons normally wants to scrub the environment.  However, sometimes
# we want to allow PATH and other variables through so that Scons
# scripts can find nacl-gcc without needing a Scons-specific argument.
if pre_base_env.Bit('use_environ'):
  pre_base_env['ENV'] = os.environ.copy()

# We want to pull CYGWIN setup in our environment or at least set flag
# nodosfilewarning. It does not do anything when CYGWIN is not involved
# so let's do it in all cases.
pre_base_env['ENV']['CYGWIN'] = os.environ.get('CYGWIN', 'nodosfilewarning')

if pre_base_env.Bit('werror'):
  werror_flags = ['-Werror']
else:
  werror_flags = []

# ----------------------------------------------------------
# Method to make sure -pedantic, etc, are not stripped from the
# default env, since occasionally an engineer will be tempted down the
# dark -- but wide and well-trodden -- path of expediency and stray
# from the path of correctness.

def EnsureRequiredBuildWarnings(env):
  if env.Bit('linux') or env.Bit('mac'):
    required_env_flags = set(['-pedantic', '-Wall'] + werror_flags)
    ccflags = set(env.get('CCFLAGS'))

    if not required_env_flags.issubset(ccflags):
      raise UserError('required build flags missing: '
                      + ' '.join(required_env_flags.difference(ccflags)))
  else:
    # windows get a pass for now
    pass

pre_base_env.AddMethod(EnsureRequiredBuildWarnings)


# ----------------------------------------------------------
# Generic Test Wrapper

# ----------------------------------------------------------
# Add list of Flaky or Bad tests to skip per platform.  A
# platform is defined as build type
# <BUILD_TYPE>-<SUBARCH>
bad_build_lists = dict()
bad_build_lists['opt-win-64'] = ['run_srpc_basic_test',
                                 'run_srpc_bad_service_test',
                                 'run_fake_browser_ppapi_test',
                                 'run_ppapi_plugin_unittest']

bad_build_lists['dbg-win-64'] = ['run_srpc_basic_test',
                                 'run_srpc_bad_service_test',
                                 'run_fake_browser_ppapi_test',
                                 'run_ppapi_plugin_unittest']

# This is a list of tests that do not yet pass when using nacl-glibc.
# TODO(mseaborn): Enable more of these tests!
nacl_glibc_skiplist = [
    # This test assumes it can allocate address ranges from the
    # dynamic code area itself.  However, this does not work when
    # ld.so assumes it can do the same.  To fix this, ld.so will need
    # an interface, like mmap() or brk(), for reserving space in the
    # dynamic code area.
    'run_dynamic_load_test',
    # This uses a canned binary that is compiled w/ newlib.  A
    # glibc version might be useful.
    'run_fuzz_nullptr_test',
    # This tests the absence of "-s" but that is no good because
    # we currently force that option on.
    'run_stubout_mode_test',
    # Struct layouts differ.
    'run_abi_test',
    # Syscall wrappers not implemented yet.
    'run_sysbasic_test',
    'run_sysbrk_test',
    # Fails because clock() is not hooked up.
    'run_timefuncs_test',
    # Needs further investigation.
    'sdk_minimal_test',
    # TODO(elijahtaylor) add apropriate syscall hooks for glibc
    'run_gc_instrumentation_test',
    # The tests below just run ncval.  ncval does not pass on
    # dynamically-linked executables because, to reduce executable
    # size, the code segment is not page-aligned, and ncval rejects
    # such executables.  See
    # http://code.google.com/p/nativeclient/issues/detail?id=1183
    'run_earth',
    'run_life',
    'run_mandel_nav',
    'run_whole_archive_test',
    # run_srpc_sysv_shm_test fails because:
    # 1) it uses fstat(), while we only have an fstat64() wrapper;
    # 2) the test needs an explicit fflush(stdout) call because the
    # process is killed without exit() being called.
    'run_srpc_sysv_shm_test',
    ]


# If a test is not in one of these suites, it will probally not be run on a
# regular basis.  These are the suites that will be run by the try bot or that
# a large number of users may run by hand.
MAJOR_TEST_SUITES = set([
  'small_tests',
  'medium_tests',
  'large_tests',
  'small_tests_arm_hw_only',
  'large_tests_arm_hw_only',
  'browser_tests',
  'huge_tests',

  # Adding a test to broken_tests means the test will not be run unless
  # explicitly requested by name. Broken tests cannot belong to any other test
  # suite and are not added to all_tests
  'broken_tests',
])

# These are the test suites we know exist, but aren't run on a regular basis.
# These test suites are essentially shortcuts that run a specific subset of the
# test cases.
ACCEPTABLE_TEST_SUITES = set([
  'validator_tests',
  'validator_modeling',
  'sel_ldr_tests',
  'sel_ldr_sled_tests',
  'barebones_tests',
  'tsan_bot_tests',
  'toolchain_tests',
  'pnacl_abi_tests',
  ])

# The major test suites are also acceptable names.  Suite names are checked
# against this set in order to catch typos.
ACCEPTABLE_TEST_SUITES.update(MAJOR_TEST_SUITES)


def ValidateTestSuiteNames(suite_name, node_name):
  if node_name is None:
    node_name = '<unknown>'

  # Prevent a silent failiure - strings are iterable!
  if not isinstance(suite_name, (list, tuple)):
    raise Exception("Test suites for %s should be specified as a list, "
      "not as a %s: %s" % (node_name, type(suite_name).__name__,
      repr(suite_name)))

  if not suite_name:
    raise Exception("No test suites are specified for %s. Put the test in "
      "'broken_tests' if there's a known issue and you don't want it to "
      "run" % (node_name,))

  if 'broken_tests' in suite_name and len(suite_name) != 1:
    raise Exception("'broken_tests' is a special test suite - if you're "
      "adding a test to it, that test (%s) should not be in any other "
      "suite" % (node_name,))

  # Make sure each test is in at least one test suite we know will run
  # 'broken_tests' is where you put tests you deliberately do not want to run
  major_suites = set(suite_name).intersection(MAJOR_TEST_SUITES)
  if not major_suites:
    raise Exception("None of the test suites %s for %s are run on a "
    "regular basis" % (repr(suite_name), node_name))

  # Make sure a wierd test suite hasn't been inadvertantly specified
  for s in suite_name:
    if s not in ACCEPTABLE_TEST_SUITES:
      raise Exception("\"%s\" is not a known test suite. Either this is "
      "a typo for %s, or it should be added to ACCEPTABLE_TEST_SUITES in "
      "SConstruct" % (s, node_name))

def IsBrokenTest(suite_name):
  return len(suite_name) == 1 and 'broken_tests' == suite_name[0]

def AddNodeToTestSuite(env, node, suite_name, node_name=None):
  build = env['BUILD_TYPE']

  # If we are testing 'NACL' we really need the trusted info
  if build=='nacl' and 'TRUSTED_ENV' in env:
    trusted_env = env['TRUSTED_ENV']
    build = trusted_env['BUILD_TYPE']
    subarch = trusted_env['BUILD_SUBARCH']
  else:
    subarch = env['BUILD_SUBARCH']

  # Build the test platform string
  platform = build + '-' + subarch

  if not node:
    return

  # There are no known-to-fail tests any more, but this code is left
  # in so that if/when we port to a new architecture or add a test
  # that is known to fail on some platform(s), we can continue to have
  # a central location to disable tests from running.  NB: tests that
  # don't *build* on some platforms need to be omitted in another way.

  # Retrieve list of tests to skip on this platform
  skiplist = bad_build_lists.get(platform,[])

  if env.Bit('nacl_glibc'):
    skiplist = skiplist + nacl_glibc_skiplist

  if node_name in skiplist:
    print '*** SKIPPING ', platform, ':', node_name
    return

  ValidateTestSuiteNames(suite_name, node_name)

  AlwaysBuild(node)
  if not IsBrokenTest(suite_name):
    env.Alias('all_tests', node)

  for s in suite_name:
    env.Alias(s, node)

  if node_name:
    env.ComponentTestOutput(node_name, node)

pre_base_env.AddMethod(AddNodeToTestSuite)

# ----------------------------------------------------------
# Convenient testing aliases
# NOTE: work around for scons non-determinism in the following two lines
Alias('sel_ldr_sled_tests', [])

Alias('small_tests', [])
Alias('medium_tests', [])
Alias('large_tests', [])
Alias('browser_tests', [])

Alias('unit_tests', 'small_tests')
Alias('smoke_tests', ['small_tests', 'medium_tests'])

Alias('memcheck_bot_tests', ['small_tests', 'medium_tests', 'large_tests'])
Alias('tsan_bot_tests', [])

# ----------------------------------------------------------
def Banner(text):
  print '=' * 70
  print text
  print '=' * 70

pre_base_env.AddMethod(Banner)

# ----------------------------------------------------------
# PLATFORM LOGIC
# ----------------------------------------------------------
# Define the build and target platforms, and use them to define the path
# for the scons-out directory (aka TARGET_ROOT)
#.
# We have "build" and "target" platforms for the non nacl environments
# which govern service runtime, validator, etc.
#
# "build" means the  platform the code is running on
# "target" means the platform the validator is checking for.
# Typically they are the same but testing it useful to have flexibility.
#
# Various variables in the scons environment are related to this, e.g.
#
# BUILD_ARCH: (arm, x86)
# BUILD_SUBARCH: (32, 64)
#
# The settings can be controlled using scons command line variables:
#
#
# buildplatform=: controls the build platform
# targetplatform=: controls the target platform
# platform=: controls both
#
# This dictionary is used to translate from a platform name to a
# (arch, subarch) pair
AVAILABLE_PLATFORMS = {
    'x86-32' : { 'arch' : 'x86' , 'subarch' : '32' },
    'x86-64' : { 'arch' : 'x86' , 'subarch' : '64' },
    'arm'    : { 'arch' : 'arm' , 'subarch' : '32' }
    }

# Look up the platform name from the command line arguments,
# defaulting to 'platform' if needed.
def GetPlatform(name):
  platform = ARGUMENTS.get(name)
  if platform is None:
    return ARGUMENTS.get('platform', 'x86-32')
  elif ARGUMENTS.get('platform') is None:
    return platform
  else:
    Banner("Can't specify both %s and %s on the command line"
           % ('platform', name))
    assert 0


# Decode platform into list [ ARCHITECTURE , EXEC_MODE ].
def DecodePlatform(platform):
  if platform in AVAILABLE_PLATFORMS:
    return AVAILABLE_PLATFORMS[platform]
  Banner("Unrecognized platform: %s" % ( platform ))
  assert 0

BUILD_NAME = GetPlatform('buildplatform')
pre_base_env.Replace(BUILD_FULLARCH=BUILD_NAME)
pre_base_env.Replace(BUILD_ARCHITECTURE=DecodePlatform(BUILD_NAME)['arch'])
pre_base_env.Replace(BUILD_SUBARCH=DecodePlatform(BUILD_NAME)['subarch'])

TARGET_NAME = GetPlatform('targetplatform')
pre_base_env.Replace(TARGET_FULLARCH=TARGET_NAME)
pre_base_env.Replace(TARGET_ARCHITECTURE=DecodePlatform(TARGET_NAME)['arch'])
pre_base_env.Replace(TARGET_SUBARCH=DecodePlatform(TARGET_NAME)['subarch'])

DeclareBit('build_x86_32', 'Building binaries for the x86-32 architecture',
           exclusive_groups='build_arch')
DeclareBit('build_x86_64', 'Building binaries for the x86-64 architecture',
           exclusive_groups='build_arch')
DeclareBit('build_arm', 'Building binaries for the ARM architecture',
           exclusive_groups='build_arch')
DeclareBit('target_x86_32', 'Tools being built will process x86-32 binaries',
           exclusive_groups='target_arch')
DeclareBit('target_x86_64', 'Tools being built will process x86-36 binaries',
           exclusive_groups='target_arch')
DeclareBit('target_arm', 'Tools being built will process ARM binaries',
           exclusive_groups='target_arch')

# Shorthand for either the 32 or 64 bit version of x86.
DeclareBit('build_x86', 'Building binaries for the x86 architecture')
DeclareBit('target_x86', 'Tools being built will process x86 binaries')

# Example: PlatformBit('build', 'x86-32') -> build_x86_32
def PlatformBit(prefix, platform):
  return "%s_%s" % (prefix, platform.replace('-', '_'))

pre_base_env.SetBits(PlatformBit('build', BUILD_NAME))
pre_base_env.SetBits(PlatformBit('target', TARGET_NAME))

if pre_base_env.Bit('build_x86_32') or pre_base_env.Bit('build_x86_64'):
  pre_base_env.SetBits('build_x86')

if pre_base_env.Bit('target_x86_32') or pre_base_env.Bit('target_x86_64'):
  pre_base_env.SetBits('target_x86')

pre_base_env.Replace(BUILD_ISA_NAME=GetPlatform('buildplatform'))

if TARGET_NAME == 'arm' and not pre_base_env.Bit('bitcode'):
  # This has always been a silent default on ARM.
  pre_base_env.SetBits('bitcode')

# Determine where the object files go
if BUILD_NAME == TARGET_NAME:
  TARGET_ROOT = '${DESTINATION_ROOT}/${BUILD_TYPE}-%s' % TARGET_NAME
else:
  TARGET_ROOT = '${DESTINATION_ROOT}/${BUILD_TYPE}-%s-to-%s' % (BUILD_NAME,
                                                                TARGET_NAME)
pre_base_env.Replace(TARGET_ROOT=TARGET_ROOT)

# TODO(robertm): eliminate this for the trust env
def FixupArmEnvironment():
  """ Glean settings by invoking setup scripts and capturing environment."""
  nacl_dir = Dir('#/').abspath
  p = subprocess.Popen([
      '/bin/bash', '-c',
      'source ' + nacl_dir +
      '/tools/llvm/setup_arm_trusted_toolchain.sh && ' +
      sys.executable + " -c 'import os ; print os.environ'"],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  (stdout, stderr) = p.communicate()
  assert p.returncode == 0, stderr
  arm_env = eval(stdout)

  for key, value in arm_env.iteritems():
    if key.startswith('NACL_') or key.startswith('ARM_'):
      os.environ[key] = value


# Source setup bash scripts and glean the settings.
if (pre_base_env.Bit('target_arm') and
    not pre_base_env.Bit('built_elsewhere') and
    ARGUMENTS.get('naclsdk_mode') != 'manual'):
  FixupArmEnvironment()

# Valgrind
pre_base_env.AddMethod(lambda self: ARGUMENTS.get('running_on_valgrind'),
                       'IsRunningUnderValgrind')

# This method indicates that the binaries we are building will validate code
# for an architecture different than the one the binaries will run on.
# NOTE Currently (2010/11/17) this is 'x86' vs. 'arm', and  x86-32 vs. x86-64
# is not considered to be a cross-tools build.
def CrossToolsBuild(env):
  return env['BUILD_ARCHITECTURE'] != env['TARGET_ARCHITECTURE']

pre_base_env.AddMethod(CrossToolsBuild, 'CrossToolsBuild')

# ----------------------------------------------------------
# PLUGIN PREREQUISITES
# ----------------------------------------------------------

PREREQUISITES = pre_base_env.Alias('plugin_prerequisites', [])


def GetPluginPrerequsites():
  # the first source is the banner. drop it
  return PREREQUISITES[0].sources[1:]

# this is a horrible hack printing all the dependencies
# dynamically accumulated for PREREQUISITES via AddPluginPrerequisite
def PluginPrerequisiteInfo(target, source, env):
  Banner("Pluging Prerequisites")
  deps =  [dep.abspath for dep in GetPluginPrerequsites()]
  print "abbreviated list: ", str([os.path.basename(d) for d in deps])
  print "full list:"
  for dep in deps:
    print dep
  return None

banner = pre_base_env.Command('plugin_prerequisites_banner', [],
                              PluginPrerequisiteInfo)

pre_base_env.Alias('plugin_prerequisites', banner)

def AddPluginPrerequisite(env, nodes):
  env.Alias('plugin_prerequisites', nodes)
  return n

pre_base_env.AddMethod(AddPluginPrerequisite)


# NOTE: PROGRAMFILES is only used for windows
#       SILENT is used to turn of user ack
INSTALL_COMMAND = ['${PYTHON}',
                   '${SCONSTRUCT_DIR}/tools/firefoxinstall.py',
                   'SILENT="%s"' % ARGUMENTS.get('SILENT', ''),
                   '"PROGRAMFILES=%s"' % os.getenv('PROGRAMFILES', ''),
                   ]

n = pre_base_env.Alias(target='firefox_install_restore',
                       source=[],
                       action=' '.join(INSTALL_COMMAND + ['MODE=RESTORE']))
AlwaysBuild(n)


n = pre_base_env.Alias(target='firefox_install_backup',
                       source=[],
                       action=' '.join(INSTALL_COMMAND + ['MODE=BACKUP']))
AlwaysBuild(n)


# ----------------------------------------------------------
def HasSuffix(item, suffix):
  if isinstance(item, str):
    return item.endswith(suffix)
  elif hasattr(item, '__getitem__'):
    return HasSuffix(item[0], suffix)
  else:
    return item.path.endswith(suffix)


def DualLibrary(env, lib_name, *args, **kwargs):
  """Builder to build both .a and _shared.a library in one step.

  Args:
    env: Environment in which we were called.
    lib_name: Library name.
    args: Positional arguments.
    kwargs: Keyword arguments.
  """
  static_objs = [i for i in Flatten(args[0]) if not HasSuffix(i, '.os')]
  shared_objs = [i for i in Flatten(args[0]) if not HasSuffix(i, '.o')]
  # Built static library as ususal.
  env.ComponentLibrary(lib_name, static_objs, **kwargs)
  # Build a static library using -fPIC for the .o's.
  if env.Bit('target_x86_64') and env.Bit('linux'):
    env_shared = env.Clone(OBJSUFFIX='.os')
    env_shared.Append(CCFLAGS=['-fPIC'])
    env_shared.ComponentLibrary(lib_name + '_shared', shared_objs, **kwargs)


def DualObject(env, *args, **kwargs):
  """Builder to build both .o and .os in one step.

  Args:
    env: Environment in which we were called.
    args: Positional arguments.
    kwargs: Keyword arguments.
  """
  # Built static library as ususal.
  ret = env.ComponentObject(*args, **kwargs)
  # Build a static library using -fPIC for the .o's.
  if env.Bit('target_x86_64') and env.Bit('linux'):
    env_shared = env.Clone(OBJSUFFIX='.os')
    env_shared.Append(CCFLAGS=['-fPIC'])
    ret += env_shared.ComponentObject(*args, **kwargs)
  return ret


def AddDualLibrary(env):
  env.AddMethod(DualLibrary)
  env.AddMethod(DualObject)
  env['SHARED_LIBS_SPECIAL'] = env.Bit('target_x86_64') and env.Bit('linux')


def InstallPlugin(target, source, env):
  Banner('Pluging Installation')
  # NOTE: sandbox settings are ignored for non-linux systems
  # TODO: we may want to enable this for more linux platforms
  if pre_base_env.Bit('build_x86_32'):
    sb = 'USE_SANDBOX=1'
  else:
    sb = 'USE_SANDBOX=0'

  deps =  [dep.abspath for dep in GetPluginPrerequsites()]
  command = env.subst(' '.join(INSTALL_COMMAND + ['MODE=INSTALL', sb] + deps))
  return os.system(command)

DeclareBit('nacl_glibc', 'Use nacl-glibc for building untrusted code')
pre_base_env.SetBitFromOption('nacl_glibc', False)

# In prebuild mode we ignore the dependencies so that stuff does
# NOT get build again
# Optionally ignore the build process.
DeclareBit('prebuilt', 'Disable all build steps, only support install steps')
pre_base_env.SetBitFromOption('prebuilt', False)


# Disable tests/platform quals that would fail on vmware + hardy + 64 as
# currently run on some of the buildbots.
DeclareBit('disable_hardy64_vmware_failures',
           'Disable tests/platform quals that would fail on '
           'vmware + hardy + 64 as currently run on some of the buildbots.')
pre_base_env.SetBitFromOption('disable_hardy64_vmware_failures', False)
if pre_base_env.Bit('disable_hardy64_vmware_failures'):
  print 'Running with --disable_hardy64_vmware_failures'


if pre_base_env.Bit('prebuilt') or pre_base_env.Bit('built_elsewhere'):
  n = pre_base_env.Command('firefox_install_command',
                           [],
                           InstallPlugin)
else:
  n = pre_base_env.Command('firefox_install_command',
                           PREREQUISITES,
                           InstallPlugin)

n = pre_base_env.Alias('firefox_install', n)
AlwaysBuild(n)


# ----------------------------------------------------------
# HELPERS FOR TEST INVOLVING TRUSTED AND UNTRUSTED ENV
# ----------------------------------------------------------
def GetEmulator(env):
  emulator = ARGUMENTS.get('force_emulator')
  if emulator is None:
    emulator = env.get('EMULATOR', '')
  return emulator


def GetValidator(env, validator):
  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    return None

  if validator is None:
    if env.Bit('build_arm'):
      validator = 'arm-ncval-core'
    else:
      validator = 'ncval'

  trusted_env = env['TRUSTED_ENV']
  return trusted_env.File('${STAGING_DIR}/${PROGPREFIX}%s${PROGSUFFIX}' %
                    validator)


def GetSelLdr(env, loader='sel_ldr'):
  sel_ldr = ARGUMENTS.get('force_sel_ldr')
  if sel_ldr:
    return sel_ldr

  # NOTE: that the variable TRUSTED_ENV is set by ExportSpecialFamilyVars()
  if 'TRUSTED_ENV' not in env:
    return None

  trusted_env = env['TRUSTED_ENV']
  sel_ldr = trusted_env.File('${STAGING_DIR}/${PROGPREFIX}%s${PROGSUFFIX}' %
                             loader)
  return sel_ldr


def CommandValidatorTestNacl(env, name, image,
                             validator_flags=None,
                             validator=None,
                             size='medium',
                             **extra):
  validator = GetValidator(env, validator)
  if validator is None:
    print 'WARNING: no validator found. Skipping test %s' % name
    return []

  if validator_flags is None:
    validator_flags = []

  command = [validator] + validator_flags + [image]
  return CommandTest(env, name, command, size, **extra)

pre_base_env.AddMethod(CommandValidatorTestNacl)

# ----------------------------------------------------------
EXTRA_ENV = [('XAUTHORITY', None),
             ('HOME', None),
             ('DISPLAY', None),
             ('SSH_TTY', None),
             ('KRB5CCNAME', None),
             ]

SELENIUM_TEST_SCRIPT = '${SCONSTRUCT_DIR}/tools/selenium_tester.py'

def BrowserTester(env,
                target,
                url,
                files,
                browser,
                log_verbosity=2,
                args=[]):

  # NOTE: hack to enable chrome testing - only works with Linux so far
  if ARGUMENTS.get('chrome_browser_path'):
    browser = env.subst('"*googlechrome '
                        '${SOURCE_ROOT}/native_client/tools/'
                        'google-chrome-wrapper.py"')
    # this env affects the behavior of google-chrome-wrapper.py
    env['ENV']['CHROME_BROWSER_EXE'] = ARGUMENTS.get('chrome_browser_path')

  deps = [SELENIUM_TEST_SCRIPT] + files
  command = ['${SOURCES[0].abspath}', '--url', url, '--browser', browser]
  for i in range(len(files)):
    command.append('--file')
    command.append('${SOURCES[%d].abspath}' % (i + 1))

  # NOTE: additional hack to enable chrome testing
  # use a more recent version of the selenium server
  if ARGUMENTS.get('chrome_browser_path'):
    command.append('--selenium_jar')
    command.append(env.subst('${SOURCE_ROOT}/third_party/selenium/'
                             'selenium-server-2.0a1/'
                             'selenium-server-standalone.jar'))

  # NOTE: setting the PYTHONPATH is currently not necessary as the test
  #       script sets its own path
  # env['ENV']['PYTHONPATH'] = ???
  # NOTE: since most of the demos use X11 we need to make sure
  #      some env vars are set for tag, val in extra_env:
  for tag, val in EXTRA_ENV:
    if val is None:
      if os.getenv(tag) is not None:
        env['ENV'][tag] = os.getenv(tag)

  # TODO(robertm): explain why this is necessary
  env['ENV']['NACL_DISABLE_SECURITY_FOR_SELENIUM_TEST'] = '1'

  node = env.Command(target, deps, ' '.join(command))
  # If we are testing build output captured from elsewhere,
  # ignore build dependencies.
  if env.Bit('built_elsewhere'):
    env.Ignore(node, deps)
  return node

pre_base_env.AddMethod(BrowserTester)

# ----------------------------------------------------------
def DemoSelLdrNacl(env,
                   target,
                   nexe,
                   log_verbosity=2,
                   sel_ldr_flags=['-d'],
                   args=[]):

  sel_ldr = GetSelLdr(env);
  if not sel_ldr:
    print 'WARNING: no sel_ldr found. Skipping test %s' % target
    return []

  deps = [sel_ldr, nexe]
  command = (['${SOURCES[0].abspath}'] + sel_ldr_flags +
             ['-f', '${SOURCES[1].abspath}', '--'] + args)

  # NOTE: since most of the demos use X11 we need to make sure
  #      some env vars are set for tag, val in extra_env:
  for tag, val in EXTRA_ENV:
    if val is None:
      if os.getenv(tag) is not None:
        env['ENV'][tag] = os.getenv(tag)
      else:
        env['ENV'][tag] =  env.subst(val)

  node = env.Command(target, deps, ' '.join(command))
  return node

# NOTE: This will not really work for ARM with user mode qemu.
#       Support would likely require some emulation magic.
pre_base_env.AddMethod(DemoSelLdrNacl)

# ----------------------------------------------------------
def CommandGdbTestNacl(env, name, command,
                       gdb_flags=[],
                       loader='sel_ldr',
                       input=None,
                       **extra):
  """Runs a test under NaCl GDB."""


  sel_ldr = GetSelLdr(env, loader);
  if not sel_ldr:
    print 'WARNING: no sel_ldr found. Skipping test %s' % name
    return []

  gdb = nacl_extra_sdk_env['GDB']
  command = ([gdb, '-q', '-batch', '-x', input, '--loader', sel_ldr] +
             gdb_flags + command)

  return CommandTest(env, name, command, 'large', **extra)

pre_base_env.AddMethod(CommandGdbTestNacl)


# ----------------------------------------------------------
def AddToStringifiedList(lst, additions):
  if not lst:
    return additions
  else:
    return additions + "," + lst

# ----------------------------------------------------------
def CommandSelLdrTestNacl(env, name, command,
                          log_verbosity=2,
                          sel_ldr_flags=None,
                          loader='sel_ldr',
                          size='medium',
                          # True for *.nexe statically linked with glibc
                          glibc_static=False,
                          **extra):
  # Disable all sel_ldr tests for windows under coverage.
  # Currently several .S files block sel_ldr from being instrumented.
  # See http://code.google.com/p/nativeclient/issues/detail?id=831
  if ('TRUSTED_ENV' in env and
      env['TRUSTED_ENV'].Bit('coverage_enabled') and
      env['TRUSTED_ENV'].Bit('windows')):
    return []

  sel_ldr = GetSelLdr(env, loader);
  if not sel_ldr:
    print 'WARNING: no sel_ldr found. Skipping test %s' % name
    return []

  if sel_ldr_flags is None:
    sel_ldr_flags = []

  # Skip platform qualification checks on configurations with known issues.
  if GetEmulator(env) or \
    env.Bit('disable_hardy64_vmware_failures') or \
    env.IsRunningUnderValgrind():
    sel_ldr_flags += ['-Q']

  if env.Bit('nacl_glibc'):
    if not glibc_static:
      command = ['${NACL_SDK_LIB}/runnable-ld.so',
                 '--library-path', '${NACL_SDK_LIB}'] + command
    # Enable file access.
    sel_ldr_flags += ['-a']
    # TODO(mseaborn): Remove the need for the -s (stub out) option.
    sel_ldr_flags += ['-s']

  command = [sel_ldr] + sel_ldr_flags + ['--'] + command

  # NOTE(robertm): log handling is a little magical
  # We do not pass these via flags because those are not usable for sel_ldr
  # when testing via plugin, esp windows.
  if 'log_golden' in extra:
    logout = '${TARGET}.log'
    extra['logout'] = logout
    extra_env  = 'NACLLOG=%s,NACLVERBOSITY=%d' % (logout, log_verbosity)
    extra['osenv'] = AddToStringifiedList(extra.get('osenv'),
                                          extra_env)
  # Add Architechture Info
  extra['arch'] = env['BUILD_ARCHITECTURE']
  extra['subarch'] = env['BUILD_SUBARCH']
  return CommandTest(env, name, command, size, **extra)

pre_base_env.AddMethod(CommandSelLdrTestNacl)

# ----------------------------------------------------------
TEST_EXTRA_ARGS = ['stdin', 'logout',
                   'stdout_golden', 'stderr_golden', 'log_golden',
                   'filter_regex', 'filter_inverse', 'filter_group_only',
                   'osenv', 'arch', 'subarch', 'exit_status']

TEST_TIME_THRESHOLD = {
    'small':   2,
    'medium': 10,
    'large':  60,
    'huge': 1800,
    }

TEST_SCRIPT = '${SCONSTRUCT_DIR}/tools/command_tester.py'

# Valgrind handles SIGSEGV in a way our testing tools do not expect.
UNSUPPORTED_VALGRIND_EXIT_STATUS = ['sigill' ,
                                    'segfault',
                                    'sigsegv_or_equivalent',
                                    'untrusted_sigill' ,
                                    'untrusted_segfault',
                                    'untrusted_sigsegv_or_equivalent',
                                    'trusted_sigill' ,
                                    'trusted_segfault',
                                    'trusted_sigsegv_or_equivalent'];


def CommandTest(env, name, command, size='small',
                direct_emulation=True, extra_deps=[], **extra):
  if not  name.endswith('.out') or name.startswith('$'):
    print "ERROR: bad  test filename for test output ", name
    assert 0

  if (env.IsRunningUnderValgrind() and
      extra.get('exit_status') in UNSUPPORTED_VALGRIND_EXIT_STATUS):
    print 'Skipping death test "%s" under Valgrind' % name
    return []

  name = '${TARGET_ROOT}/test_results/' + name
  # NOTE: using the long version of 'name' helps distinguish opt vs dbg
  max_time = TEST_TIME_THRESHOLD[size]
  scale_timeout = ARGUMENTS.get('scale_timeout')
  if scale_timeout:
    max_time = max_time * int(scale_timeout)

  script_flags = ['--name', name,
                  '--report', name,
                  '--time_warning', str(max_time),
                  '--time_error', str(10 * max_time),
                  ]

  run_under = ARGUMENTS.get('run_under')
  if run_under:
    run_under_extra_args = ARGUMENTS.get('run_under_extra_args')
    if run_under_extra_args:
      run_under = run_under + ',' + run_under_extra_args
    script_flags.append('--run_under');
    script_flags.append(run_under);

  deps = [TEST_SCRIPT]

  # extract deps from command and rewrite
  for n, c in enumerate(command):
    if not isinstance(c, str):
      if len(Flatten(c)) != 1:
        # Do not allow this, because it would cause "deps" to get out
        # of sync with the indexes in "command".
        # See http://code.google.com/p/nativeclient/issues/detail?id=1086
        raise AssertionError('Argument to CommandTest() actually contains '
                             'multiple arguments: %r' % c)
      deps.append(c)
      command[n] = '${SOURCES[%d].abspath}' % (len(deps) - 1)

  emulator = GetEmulator(env)
  if emulator:
    if direct_emulation:
      command = [emulator] + command
    else:
      extra_env = 'EMULATOR=%s' %  env['EMULATOR'].replace(' ', r'\ ')
      extra['osenv'] = AddToStringifiedList(extra.get('osenv'),
                                            extra_env)

  # extract deps from flags and rewrite
  for flag_name, flag_value in extra.iteritems():
    assert flag_name in TEST_EXTRA_ARGS
    if not isinstance(flag_value, str):
      deps.append(flag_value)
      flag_value = '${SOURCES[%d].abspath}' % (len(deps) - 1)
    script_flags.append('--' + flag_name)
    script_flags.append(flag_value)


  # NOTE: "SOURCES[X]" references the scons object in deps[x]
  command = ['${PYTHON}',
             '${SOURCES[0].abspath}',
             ' '.join(script_flags),
             ' '.join(command),
             ]

  # If we are testing build output captured from elsewhere,
  # ignore build dependencies.
  if env.Bit('built_elsewhere'):
    env.Ignore(name, deps)
  else:
    env.Depends(name, extra_deps)

  return env.Command(name, deps, ' '.join(command))

pre_base_env.AddMethod(CommandTest)

def AliasSrpc(env, alias, is_client, build_dir, srpc_files,
              name, h_file, cc_file, guard):
  if is_client:
    direction = '-c '
  else:
    direction = '-s '

  # The include path used must be the original passed in directory.
  include = h_file

  # But for the output path prepend the output directory.
  if build_dir:
    h_file = build_dir + '/' + h_file
    cc_file = build_dir + '/' + cc_file

  # Build the base command-line
  gen_args = ['${PYTHON}', '${SOURCE_ROOT}/native_client/tools/srpcgen.py',
              '--include=' + include, direction, name, guard, h_file, cc_file]

  # Add the srpc_files
  for name in srpc_files:
    gen_args.append(File(name).srcnode().abspath)

  # Build the command line string
  action = ' '.join(gen_args)

  # Force all Unix style paths to be OS appropriate
  action = os.path.sep.join(action.split('/'))
  node = env.Alias(alias, source=srpc_files, action=[Action(action)])
  return node


def TrustedSrpc(env, is_client, srpc_files, name, h_file, cc_file, guard):
  # Add these SRPC files to the 'srpcgen' alias for a manual build step.
  h_file = 'trusted/srpcgen/' + h_file
  env.AliasSrpc(alias='srpcgen', is_client=is_client,
                build_dir='${SOURCE.srcdir}', srpc_files=srpc_files,
                name=name, h_file=h_file, cc_file=cc_file, guard=guard)
  env.AlwaysBuild(env.Alias('srpcgen'))

  # For trusted SRPC autogenerated files we must verify that they are
  # up to date, so we generate a second copy of the files to the output
  # directory which we will use to diff against the above copy which is
  # checked in and only generated mnaully by calling the 'srgcgen' target.
  env.AliasSrpc(alias='srpcdif', is_client=is_client,
                build_dir='${TARGET_ROOT}/srpcgen/${SOURCE.srcdir}',
                srpc_files=srpc_files, name=name, h_file=h_file,
                cc_file=cc_file, guard=guard)
  env.AlwaysBuild(env.Alias('srpcdif'))


def UntrustedSrpc(env, is_client, srpc_files, name, h_file, cc_file, guard):
  h_file = 'untrusted/srpcgen/' + h_file
  env.AliasSrpc(alias='srpcgen', is_client=is_client,
                build_dir='${SOURCE.srcdir}', srpc_files=srpc_files,
                name=name, h_file=h_file, cc_file=cc_file, guard=guard)
  env.AlwaysBuild(env.Alias('srpcgen'))

pre_base_env.AddMethod(AliasSrpc)
pre_base_env.AddMethod(TrustedSrpc)
pre_base_env.AddMethod(UntrustedSrpc)

# ----------------------------------------------------------
def GetPrintableCommandName(cmd):
  """Look at the first few elements of cmd to derive a suitable command name."""
  cmd_tokens = cmd.split()
  if "python" in cmd_tokens[0]:
    cmd_name = cmd_tokens[1]
  else:
    cmd_name = cmd_tokens[0].split('(')[0]

  # undo some pretty printing damage done by hammer
  cmd_name = cmd_name.replace('________','')
  # use file name part of a path
  return cmd_name.split('/')[-1]


def GetPrintableEnvironmentName(env):
  # use file name part of a obj root path as env name
  return env.subst('${TARGET_ROOT}').split('/')[-1]


CMD_COUNTER = {}
ENV_COUNTER = {}


def CustomCommandPrinter(cmd, targets, source, env):
  # Abuse the print hook to count the commands that are executed
  if env.Bit('target_stats'):
    cmd_name = GetPrintableCommandName(cmd)
    env_name = GetPrintableEnvironmentName(env)
    CMD_COUNTER[cmd_name] = CMD_COUNTER.get(cmd_name, 0) + 1
    ENV_COUNTER[env_name] = ENV_COUNTER.get(env_name, 0) + 1

  if env.Bit('pp'):
    # Our pretty printer
    if targets:
      cmd_name = GetPrintableCommandName(cmd)
      env_name = GetPrintableEnvironmentName(env)
      sys.stdout.write('[%s] [%s] %s\n' % (cmd_name, env_name,
                                           targets[0].get_path()))
  else:
    # The SCons default (copied from print_cmd_line in Action.py)
    sys.stdout.write(cmd + u'\n')

pre_base_env.Append(PRINT_CMD_LINE_FUNC=CustomCommandPrinter)

# ----------------------------------------------------------
# for generation of a promiscuous sel_ldr
# ----------------------------------------------------------
if pre_base_env.Bit('dangerous_debug_disable_inner_sandbox'):
  pre_base_env.Append(
      # NOTE: this also affects .S files
      CPPDEFINES=['DANGEROUS_DEBUG_MODE_DISABLE_INNER_SANDBOX'],
      )

# ----------------------------------------------------------
DeclareBit('chrome',
           'Build the plugin as a static library to be linked with Chrome')
pre_base_env.SetBitFromOption('chrome', False)
if pre_base_env.Bit('chrome'):
  pre_base_env.Append(
    CPPDEFINES = [
        ['CHROME_BUILD', 1],
    ],
  )
  # To build for Chrome sdl=none must be used
  ARGUMENTS['sdl'] = 'none'
else:
  pre_base_env.Append(
    CPPDEFINES = [
        ['NACL_STANDALONE', 1],
    ],
  )

# ----------------------------------------------------------
def CheckPlatformPreconditions():
  "Check and fail fast if platform-specific preconditions are unmet."

  if base_env.Bit('target_arm') and (base_env.Bit('build_x86_32') or
                                     base_env.Bit('build_x86_64')):
      assert os.getenv('NACL_SDK_CC'), (
          "NACL_SDK_CC undefined. "
          "Source tools/llvm/setup_arm_untrusted_toolchain.sh.")

# ----------------------------------------------------------
base_env = pre_base_env.Clone()
base_env.Append(
  BUILD_SUBTYPE = '',
  CPPDEFINES = [
    ['NACL_BLOCK_SHIFT', '5'],
    ['NACL_BLOCK_SIZE', '32'],
    ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
    ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
    ['NACL_TARGET_ARCH', '${TARGET_ARCHITECTURE}' ],
    ['NACL_TARGET_SUBARCH', '${TARGET_SUBARCH}' ],
    ],
  CPPPATH = ['${SOURCE_ROOT}'],

  EXTRA_CFLAGS = [],
  EXTRA_CCFLAGS = [],
  EXTRA_CXXFLAGS = [],
  EXTRA_LIBS = [],
  CFLAGS = ['${EXTRA_CFLAGS}'],
  CCFLAGS = ['${EXTRA_CCFLAGS}'],
  CXXFLAGS = ['${EXTRA_CXXFLAGS}'],
  LIBS = ['${EXTRA_LIBS}'],
)

base_env.Append(
  BUILD_SCONSCRIPTS = [
    # KEEP THIS SORTED PLEASE
    'src/shared/gio/build.scons',
    'src/shared/imc/build.scons',
    'src/shared/npruntime/build.scons',
    'src/shared/platform/build.scons',
    'src/shared/ppapi/build.scons',
    'src/shared/ppapi_proxy/build.scons',
    'src/shared/srpc/build.scons',
    'src/shared/utils/build.scons',
    'src/third_party_mod/gtest/build.scons',
    'src/tools/validator_tools/build.scons',
    'src/trusted/debug_stub/build.scons',
    'src/trusted/desc/build.scons',
    'src/trusted/expiration/build.scons',
    'src/trusted/gdb_rsp/build.scons',
    'src/trusted/handle_pass/build.scons',
    'src/trusted/nacl_base/build.scons',
    'src/trusted/nonnacl_util/build.scons',
    'src/trusted/perf_counter/build.scons',
    'src/trusted/platform_qualify/build.scons',
    'src/trusted/plugin/build.scons',
    'src/trusted/sandbox/build.scons',
    'src/trusted/sel_universal/build.scons',
    'src/trusted/service_runtime/build.scons',
    # TODO: This file has an early out in case we are building for ARM
    #       but provides nchelper lib. Needs to be cleaned up
    'src/trusted/validator_x86/build.scons',
    'tests/python_version/build.scons',
    'tests/selenium_self_test/build.scons',
    'tests/tools/build.scons',
    'tests/unittests/shared/srpc/build.scons',
    'tests/unittests/shared/imc/build.scons',
    'tests/unittests/shared/platform/build.scons',
    'installer/build.scons'
    ],
  )

# TODO(adonovan): re-enable this and test it once the build is fixed.
# CheckPlatformPreconditions()

# The ARM validator can be built for any target that doesn't use ELFCLASS64.
if not base_env.Bit('target_x86_64'):
  base_env.Append(
      BUILD_SCONSCRIPTS = [
        'src/trusted/validator_arm/build.scons',
      ])

base_env.Replace(
    NACL_BUILD_FAMILY = 'TRUSTED',

    SDL_HERMETIC_LINUX_DIR='${MAIN_DIR}/../third_party/sdl/linux/v1_2_13',
    SDL_HERMETIC_MAC_DIR='${MAIN_DIR}/../third_party/sdl/osx/v1_2_13',
    SDL_HERMETIC_WINDOWS_DIR='${MAIN_DIR}/../third_party/sdl/win/v1_2_13',
)

# Add optional scons files if present in the directory tree.
if os.path.exists(pre_base_env.subst('${MAIN_DIR}/supplement/build.scons')):
  base_env.Append(BUILD_SCONSCRIPTS=['${MAIN_DIR}/supplement/build.scons'])

# Select tests to run under coverage build.
base_env['COVERAGE_TARGETS'] = ['small_tests', 'medium_tests']

base_env.Help("""\
======================================================================
Help for NaCl
======================================================================

Common tasks:
-------------

* cleaning:           scons -c
* building:           scons
* just the doc:       scons --mode=doc
* build mandel:       scons --mode=nacl mandel.nexe
* smoke test:         scons --mode=nacl,opt-linux -k pp=1 smoke_tests

* sel_ldr:            scons --mode=opt-linux sel_ldr

* build the plugin:         scons --mode=opt-linux npGoogleNaClPlugin
*      or:                  scons --mode=opt-linux src/trusted/plugin
* install the plugin:       scons --verbose firefox_install
* install pre-built plugin: scons --prebuilt firefox_install

* build libs needed by sdk: scons --mode=nacl_extra_sdk extra_sdk_update
* purge libs needed by sdk: scons --mode=nacl_extra_sdk extra_sdk_clean
* rebuild sdk:              scons --mode=nacl_extra_sdk \
extra_sdk_clean \
extra_sdk_update_header \
install_libpthread \
extra_sdk_update

* dump system info:   scons --mode=nacl,opt-linux dummy

Options:
--------

sdl=<mode>        where <mode>:

                    'none': don't use SDL (default)
                    'local': use locally installed SDL
                    'hermetic': use the hermetic SDL copy

naclsdk_mode=<mode>   where <mode>:

                    'local': use locally installed sdk kit
                    'download': use the download copy (default)
                    'custom:<path>': use kit at <path>
                    'manual': use settings from env vars NACL_SDL_xxx


--prebuilt          Do not build things, just do install steps

--verbose           Full command line logging before command execution

pp=1                Use command line pretty printing (more concise output)

sysinfo=1           Verbose system info printing

naclsdk_validate=0  Suppress presence check of sdk



Automagically generated help:
-----------------------------
""")

# ---------------------------------------------------------
def GenerateOptimizationLevels(env):
  # Generate debug variant.
  debug_env = env.Clone(tools = ['target_debug'])
  debug_env['OPTIMIZATION_LEVEL'] = 'dbg'
  debug_env['BUILD_TYPE'] = debug_env.subst('$BUILD_TYPE')
  debug_env['BUILD_DESCRIPTION'] = debug_env.subst('$BUILD_DESCRIPTION')
  AddDualLibrary(debug_env)
  # Add to the list of fully described environments.
  environment_list.append(debug_env)

  # Generate opt variant.
  opt_env = env.Clone(tools = ['target_optimized'])
  opt_env['OPTIMIZATION_LEVEL'] = 'opt'
  opt_env['BUILD_TYPE'] = opt_env.subst('$BUILD_TYPE')
  opt_env['BUILD_DESCRIPTION'] = opt_env.subst('$BUILD_DESCRIPTION')
  AddDualLibrary(opt_env)
  # Add opt to the default group.
  opt_env.Append(BUILD_GROUPS = ['default'])
  # Add to the list of fully described environments.
  environment_list.append(opt_env)

  return (debug_env, opt_env)

if base_env.Bit('build_x86_64'):
  base_env['WINASM'] = 'x86_64-w64-mingw32-as.exe'
else:
  base_env['WINASM'] = 'as'


# ----------------------------------------------------------
windows_env = base_env.Clone(
    BUILD_TYPE = '${OPTIMIZATION_LEVEL}-win',
    BUILD_TYPE_DESCRIPTION = 'Windows ${OPTIMIZATION_LEVEL} build',
    tools = ['target_platform_windows'],
    # Windows /SAFESEH linking requires either an .sxdata section be present or
    # that @feat.00 be defined as a local, abolute symbol with an odd value.
    ASCOM = '$ASPPCOM /E | $WINASM -defsym @feat.00=1 -o $TARGET',
    PDB = '${TARGET.base}.pdb',
    # Strict doesnt't currently work for windows since some of the system
    # libraries like wsock32 are magical.
    LIBS_STRICT = False,
)

windows_env.Append(
    CPPDEFINES = [
        ['NACL_WINDOWS', '1'],
        ['NACL_OSX', '0'],
        ['NACL_LINUX', '0'],
        ['_WIN32_WINNT', '0x0501'],
        ['__STDC_LIMIT_MACROS', '1'],
        ['NOMINMAX', '1'],
    ],
    LIBS = ['wsock32', 'advapi32'],
    CCFLAGS = ['/EHsc', '/WX'],
    # TODO(new_hire): '/Wp64' should be in here.
)

# This linker option allows us to ensure our builds are compatible with
# Chromium, which uses it.
if windows_env.Bit('build_x86_32'):
  windows_env.Append(LINKFLAGS = "/safeseh")

windows_env['ENV']['PATH'] = os.environ.get('PATH', '[]')
windows_env['ENV']['INCLUDE'] = os.environ.get('INCLUDE', '[]')
windows_env['ENV']['LIB'] = os.environ.get('LIB', '[]')
windows_env.AppendENVPath('PATH',
    '$SOURCE_ROOT/third_party/gnu_binutils/files')
windows_env.AppendENVPath('PATH',
    '$SOURCE_ROOT/third_party/mingw-w64/mingw/bin')

(windows_debug_env,
 windows_optimized_env) = GenerateOptimizationLevels(windows_env)

if ARGUMENTS.get('sdl', 'hermetic') != 'none':
  # These will only apply to sdl!=none builds!
  windows_debug_env.Append(CPPDEFINES = ['_DLL', '_MT'])
  windows_optimized_env.Append(CPPDEFINES = ['_DLL', '_MT'])
  # SDL likes DLLs
  if '/MT' in windows_optimized_env['CCFLAGS']:
    windows_optimized_env.FilterOut(CCFLAGS=['/MT']);
    windows_optimized_env.Append(CCFLAGS=['/MD']);
  if '/MTd' in windows_debug_env['CCFLAGS']:
    windows_debug_env.FilterOut(CCFLAGS=['/MTd']);
    windows_debug_env.Append(CCFLAGS=['/MDd']);
    # this doesn't feel right, but fixes dbg-win
    windows_debug_env.Append(LINKFLAGS = ['/NODEFAULTLIB:msvcrt'])
  # make source level debugging a little easier
  if '/Z7' not in windows_debug_env['CCFLAGS']:
    if '/Zi' not in windows_debug_env['CCFLAGS']:
      windows_debug_env.Append(CCFLAGS=['/Z7'])

# TODO(bradnelson): This does not quite work yet (c.f.
# service_runtime/build.scons)
# move some debug info close to the binaries where the debugger can find it
# This only matter when we the "wrong" version of VisualStudio is used
n = windows_debug_env.Command('dummy', [], 'echo INSTALL WINDOWS DEBUG DATA')
windows_debug_env.Alias('windows_debug_data_install', n)

n = windows_debug_env.Replicate(
    '${STAGING_DIR}',
    '${VC80_DIR}/vc/redist/Debug_NonRedist/x86/Microsoft.VC80.DebugCRT')

windows_debug_env.Alias('windows_debug_data_install', n)

n = windows_debug_env.Replicate(
    '${STAGING_DIR}',
    '${VC80_DIR}/vc/redist/x86/Microsoft.VC80.CRT')

windows_debug_env.Alias('windows_debug_data_install', n)
# ----------------------------------------------------------

unix_like_env = base_env.Clone()
# -Wdeclaration-after-statement is desirable because MS studio does
# not allow declarations after statements in a block, and since much
# of our code is portable and primarily initially tested on Linux,
# it'd be nice to get the build error earlier rather than later
# (building and testing on Linux is faster).
unix_like_env.Prepend(
  CFLAGS = ['-std=gnu99', '-Wdeclaration-after-statement' ],
  CCFLAGS = [
    # '-malign-double',
    '-Wall',
    '-pedantic',
    '-Wextra',
    '-Wno-long-long',
    '-Wswitch-enum',
    '-Wsign-compare',
    '-fvisibility=hidden',
  ] + werror_flags,
  CXXFLAGS=['-std=c++98'],
  LIBPATH=['/usr/lib'],
  LIBS = ['pthread'],
  CPPDEFINES = [['__STDC_LIMIT_MACROS', '1'],
                ['__STDC_FORMAT_MACROS', '1'],
                ],
)

if unix_like_env.Bit('use_libcrypto'):
  unix_like_env.Append(LIBS=['crypto'])
else:
  unix_like_env.Append(CFLAGS=['-DUSE_CRYPTO=0'])

# ----------------------------------------------------------

mac_env = unix_like_env.Clone(
    BUILD_TYPE = '${OPTIMIZATION_LEVEL}-mac',
    BUILD_TYPE_DESCRIPTION = 'MacOS ${OPTIMIZATION_LEVEL} build',
    tools = ['target_platform_mac'],
    # TODO(bradnelson): this should really be able to live in unix_like_env
    #                   but can't due to what the target_platform_x module is
    #                   doing.
    LINK = '$CXX',
    PLUGIN_SUFFIX = '.bundle',
)

if mac_env.Bit('build_x86_64'):
  # OS X 10.5 was the first version to support x86-64.  We need to
  # specify 10.5 rather than 10.4 here otherwise building
  # get_plugin_dirname.mm gives the warning (converted to an error)
  # "Mac OS X version 10.5 or later is needed for use of the new objc abi".
  mac_env.Append(
      CCFLAGS=['-mmacosx-version-min=10.5'],
      LINKFLAGS=['-mmacosx-version-min=10.5'],
      CPPDEFINES=[['MAC_OS_X_VERSION_MIN_REQUIRED', 'MAC_OS_X_VERSION_10_5']])
else:
  mac_env.Append(
      CCFLAGS=['-mmacosx-version-min=10.4'],
      LINKFLAGS=['-mmacosx-version-min=10.4'],
      CPPDEFINES=[['MAC_OS_X_VERSION_MIN_REQUIRED', 'MAC_OS_X_VERSION_10_4']])
subarch_flag = '-m%s' % mac_env['BUILD_SUBARCH']
mac_env.Append(
    CCFLAGS=[subarch_flag, '-fPIC'],
    ASFLAGS=[subarch_flag],
    LINKFLAGS=[subarch_flag, '-fPIC'],
    CPPDEFINES = [['NACL_WINDOWS', '0'],
                  ['NACL_OSX', '1'],
                  ['NACL_LINUX', '0'],
                  # defining _DARWIN_C_SOURCE breaks 10.4
                  #['_DARWIN_C_SOURCE', '1'],
                  #['__STDC_LIMIT_MACROS', '1']
                  ],
)

(mac_debug_env, mac_optimized_env) = GenerateOptimizationLevels(mac_env)

# ----------------------------------------------------------
EMULATOR = None

linux_env = unix_like_env.Clone(
    BUILD_TYPE = '${OPTIMIZATION_LEVEL}-linux',
    BUILD_TYPE_DESCRIPTION = 'Linux ${OPTIMIZATION_LEVEL} build',
    tools = ['target_platform_linux'],
    # TODO(bradnelson): this should really be able to live in unix_like_env
    #                   but can't due to what the target_platform_x module is
    #                   doing.
    LINK = '$CXX',
)

# -m32 and -L/usr/lib32 are needed to do 32-bit builds on 64-bit
# user-space machines; requires ia32-libs-dev to be installed; or,
# failing that, ia32-libs and symbolic links *manually* created for
# /usr/lib32/libssl.so and /usr/lib32/libcrypto.so to the current
# /usr/lib32/lib*.so.version (tested with ia32-libs 2.2ubuntu11; no
# ia32-libs-dev was available for testing).
# Additional symlinks of this sort are needed for gtk,
# see src/trusted/nonnacl_util/build.scons.

linux_env.SetDefault(
    # NOTE: look into http://www.scons.org/wiki/DoxygenBuilder
    DOXYGEN = ARGUMENTS.get('DOXYGEN', '/usr/bin/doxygen'),
)

# Prepend so we can disable warnings via Append
linux_env.Prepend(
    CPPDEFINES = [['NACL_WINDOWS', '0'],
                  ['NACL_OSX', '0'],
                  ['NACL_LINUX', '1'],
                  ['_BSD_SOURCE', '1'],
                  ['_POSIX_C_SOURCE', '199506'],
                  ['_XOPEN_SOURCE', '600'],
                  ['_GNU_SOURCE', '1'],
                  ['_LARGEFILE64_SOURCE', '1'],
                  ],
    LIBS = ['rt'],
)

if linux_env.Bit('build_x86_32'):
  linux_env.Prepend(
      ASFLAGS = ['-m32', ],
      CCFLAGS = ['-m32', ],
      LINKFLAGS = ['-m32', '-L/usr/lib32'],
      )
elif linux_env.Bit('build_x86_64'):
  linux_env.Prepend(
      ASFLAGS = ['-m64', ],
      CCFLAGS = ['-m64', ],
      LINKFLAGS = ['-m64', '-L/usr/lib64'],
      )
elif linux_env.Bit('build_arm'):
  # NOTE: this hack allows us to propagate the emulator to the untrusted env
  # TODO(robertm): clean this up
  EMULATOR = os.getenv('ARM_EMU', '')
  linux_env.Replace(CC=os.getenv('ARM_CC', 'NO-ARM-CC-SPECIFIED'),
                    CXX=os.getenv('ARM_CXX', 'NO-ARM-CXX-SPECIFIED'),
                    LD=os.getenv('ARM_LD', 'NO-ARM-LD-SPECIFIED'),
                    EMULATOR=EMULATOR,
                    ASFLAGS=[],
                    LIBPATH=['${LIB_DIR}',
                             os.getenv('ARM_LIB_DIR', '').split()],
                    LINKFLAGS=os.getenv('ARM_LINKFLAGS', ''),
                    )

  linux_env.Append(LIBS=['rt', 'dl', 'pthread', 'crypto'],
                   CCFLAGS=['-march=armv6'])
else:
  Banner('Strange platform: %s' % BUILD_NAME)

# Ensure that the executable does not get a PT_GNU_STACK header that
# causes the kernel to set the READ_IMPLIES_EXEC personality flag,
# which disables NX page protection.  This is Linux-specific.
linux_env.Prepend(LINKFLAGS=['-Wl,-z,noexecstack'])

# TODO(robert): support for arm builds

(linux_debug_env, linux_optimized_env) = GenerateOptimizationLevels(linux_env)

# Do this before the site_scons/site_tools/naclsdk.py stuff to pass it along.
pre_base_env.Append(
    PNACL_BCLDFLAGS = ARGUMENTS.get('pnacl_bcldflags', '').split(':'))

# ----------------------------------------------------------
# The nacl_env is used to build native_client modules
# using a special tool chain which produces platform
# independent binaries
# NOTE: this loads stuff from: site_scons/site_tools/naclsdk.py
# ----------------------------------------------------------
nacl_env = pre_base_env.Clone(
    tools = ['naclsdk'],
    BUILD_TYPE = 'nacl',
    BUILD_TYPE_DESCRIPTION = 'NaCl module build',
    BUILD_GROUPS = ['default'],
    NACL_BUILD_FAMILY = 'UNTRUSTED',

    EXTRA_CFLAGS = [],
    EXTRA_CCFLAGS = ARGUMENTS.get('nacl_ccflags', '').split(':'),
    EXTRA_CXXFLAGS = [],
    EXTRA_LIBS = [],
    EXTRA_LINKFLAGS = ARGUMENTS.get('nacl_linkflags', '').split(':'),

    # always optimize binaries
    # Command line option nacl_ccflags=... add additional option to nacl build
    CCFLAGS = ['-O2',
               '-fomit-frame-pointer',
               '-Wall',
               '-fdiagnostics-show-option',
               '-pedantic'] +
              werror_flags +
              ['${EXTRA_CCFLAGS}'] ,
    CPPPATH = ['$SOURCE_ROOT'],
    CFLAGS = ['${EXTRA_CFLAGS}'],
    CXXFLAGS = ['${EXTRA_CXXFLAGS}'],
    LIBS = ['${EXTRA_LIBS}'],
    LINKFLAGS = ['${EXTRA_LINKFLAGS}'],
    CPPDEFINES = [
      # _GNU_SOURCE ensures that strtof() gets declared.
      ['_GNU_SOURCE', 1],
      # This ensures that PRId64 etc. get defined.
      ['__STDC_FORMAT_MACROS', '1'],
      ],
)

if nacl_env.Bit('bitcode'):
  target_root = nacl_env.subst('${TARGET_ROOT}') + '-pnacl'
  nacl_env.Replace(TARGET_ROOT=target_root)

if nacl_env.Bit('nacl_pic'):
  target_root = nacl_env.subst('${TARGET_ROOT}') + '-pic'
  nacl_env.Replace(TARGET_ROOT=target_root)


if nacl_env.Bit('running_on_valgrind'):
  nacl_env.Append(CCFLAGS = ['-g', '-Wno-overlength-strings',
                             '-fno-optimize-sibling-calls'],
                  CPPDEFINES = [['DYNAMIC_ANNOTATIONS_ENABLED', '1' ],
                                ['DYNAMIC_ANNOTATIONS_PREFIX', 'NACL_' ]],
                  LINKFLAGS = ['-Wl,-u,have_nacl_valgrind_interceptors'],
                  LIBS = ['valgrind'])

if not nacl_env.Bit('bitcode'):
  if nacl_env.Bit('build_x86_32'):
    nacl_env.Append(CCFLAGS = ['-m32'], LINKFLAGS = '-m32')
  elif nacl_env.Bit('build_x86_64'):
    nacl_env.Append(CCFLAGS = ['-m64'], LINKFLAGS = '-m64')

if nacl_env.Bit('bitcode'):
  # TODO(robertm): remove this ASAP, we currently have llvm issue with c++
  nacl_env.FilterOut(CCFLAGS = ['-Werror'])
  nacl_env.Append(CFLAGS = werror_flags)

  # NOTE: we change the linker command line to make it possible to
  #       sneak in startup and cleanup code
  nacl_env.Prepend(EMULATOR=EMULATOR)

environment_list.append(nacl_env)

nacl_env.Append(
    BUILD_SCONSCRIPTS = [
    ####  ALPHABETICALLY SORTED ####
    'src/trusted/service_runtime/nacl.scons',
    'tests/app_lib/nacl.scons',
    'tests/autoloader/nacl.scons',
    'tests/barebones/nacl.scons',
    'tests/bundle_size/nacl.scons',
    'tests/contest_issues/nacl.scons',
    'tests/data_not_executable/nacl.scons',
    'tests/dynamic_code_loading/nacl.scons',
    'tests/dynamic_linking/nacl.scons',
    'tests/egyptian_cotton/nacl.scons',
    'tests/environment_variables/nacl.scons',
    'tests/fake_browser/nacl.scons',
    'tests/fake_browser_ppapi/nacl.scons',
    'tests/fib/nacl.scons',
    'tests/file/nacl.scons',
    'tests/gc_instrumentation/nacl.scons',
    'tests/glibc_static_test/nacl.scons',
    'tests/glibc_syscall_wrappers/nacl.scons',
    'tests/hello_world/nacl.scons',
    'tests/imc_shm_mmap/nacl.scons',
    'tests/imc_sockets/nacl.scons',
    'tests/inbrowser_test_runner/nacl.scons',
    'tests/libc_free_hello_world/nacl.scons',
    'tests/loop/nacl.scons',
    'tests/mandel/nacl.scons',
    'tests/math/nacl.scons',
    'tests/memcheck_test/nacl.scons',
    'tests/mmap/nacl.scons',
    'tests/nacl_log/nacl.scons',
    'tests/nanosleep/nacl.scons',
    'tests/native_worker/nacl.scons',
    'tests/noop/nacl.scons',
    'tests/npapi_bridge/nacl.scons',
    'tests/npapi_geturl/nacl.scons',
    'tests/npapi_hw/nacl.scons',
    'tests/npapi_pi/nacl.scons',
    'tests/npapi_runtime/nacl.scons',
    'tests/nrd_xfer/nacl.scons',
    'tests/nthread_nice/nacl.scons',
    'tests/null/nacl.scons',
    'tests/nullptr/nacl.scons',
    'tests/pepper_plugin/nacl.scons',
    'tests/pnacl_abi/nacl.scons',
    'tests/ppapi_geturl/nacl.scons',
    'tests/ppapi_proxy/nacl.scons',
    # uncomment this test once issue
    # http://code.google.com/p/nativeclient/issues/detail?id=902 gets fixed
    # 'tests/ppapi_tests/nacl.scons',
    'tests/ppapi/nacl.scons',
    'tests/rodata_not_writable/nacl.scons',
    'tests/signal_handler/nacl.scons',
    'tests/srpc/nacl.scons',
    'tests/srpc_hw/nacl.scons',
    'tests/srpc_message/nacl.scons',
    'tests/srpc_without_pthread/nacl.scons',
    'tests/stack_alignment/nacl.scons',
    'tests/stubout_mode/nacl.scons',
    'tests/sysbasic/nacl.scons',
    'tests/syscalls/nacl.scons',
    'tests/syscalls_deprecated/nacl.scons',
    'tests/syscall_return_sandboxing/nacl.scons',
    'tests/threads/nacl.scons',
    'tests/time/nacl.scons',
    'tests/toolchain/nacl.scons',
    'tests/unittests/shared/imc/nacl.scons',
    'tests/unittests/shared/srpc/nacl.scons',
    ####  ALPHABETICALLY SORTED ####
    ])

# NOTE: DEFAULT OFF
if nacl_env.Bit('build_vim'):
  nacl_env.Append(
    BUILD_SCONSCRIPTS = [
    'tests/vim/nacl.scons',
    ])

# NOTE: DEFAULT ON
if nacl_env.Bit('build_av_apps'):
  nacl_env.Append(
      BUILD_SCONSCRIPTS = [
      ####  ALPHABETICALLY SORTED ####
      'common/console/nacl.scons',

      'tests/earth/nacl.scons',
      'tests/life/nacl.scons',
      'tests/mandel_nav/nacl.scons',
      'tests/many/nacl.scons',
      # TODO(polina,dspringer): *** Add this test back in when URLLoader
      # works! ***
      # 'tests/multiarch/nacl.scons',
      'tests/tone/nacl.scons',
      'tests/voronoi/nacl.scons',
      ])

nacl_env.Append(
    BUILD_SCONSCRIPTS = [
      'tools/tests/nacl.scons',
    ])

# ----------------------------------------------------------
# Possibly install an sdk by downloading it
# ----------------------------------------------------------
# TODO: explore using a less heavy weight mechanism
# NOTE: this uses stuff from: site_scons/site_tools/naclsdk.py
import SCons.Script

SCons.Script.AddOption('--download',
                       dest='download',
                       metavar='DOWNLOAD',
                       default=False,
                       action='store_true',
                       help='deprecated - allow tools to download')

if nacl_env.GetOption('download'):
  print '@@@@ --download is deprecated, use gclient runhooks --force'
  nacl_sync_env = nacl_env.Clone()
  nacl_sync_env['ENV'] = os.environ
  nacl_sync_env.Execute('gclient runhooks --force')

# ----------------------------------------------------------
# Sanity check whether we are ready to build nacl modules
# ----------------------------------------------------------
# NOTE: this uses stuff from: site_scons/site_tools/naclsdk.py
if nacl_env.Bit('naclsdk_validate'):
  nacl_env.ValidateSdk()

# ----------------------------------------------------------
# Update prebuilt nexes
# ----------------------------------------------------------
# Contains all binaries that have to be checked in
nacl_env.Alias('prebuilt_binaries_update', [])


def AddPrebuiltBinaryToRepository(env, nodes):
  platform = GetPlatform('targetplatform')
  if platform == 'x86-32':
    path = '${SCONSTRUCT_DIR}/tests/prebuilt/x86/'
  elif platform == 'x86-64':
    path = '${SCONSTRUCT_DIR}/tests/prebuilt/x64/'
  elif platform == 'arm':
    path = '${SCONSTRUCT_DIR}/tests/prebuilt/arm/'
  n = env.Replicate(path, nodes)
  env.Alias('prebuilt_binaries_update', n)
  return n

nacl_env.AddMethod(AddPrebuiltBinaryToRepository)
# ----------------------------------------------------------
# We force this into a separate env so that the tests in nacl_env
# have NO access to any libraries build here but need to link them
# from the sdk libdir
# ----------------------------------------------------------
nacl_extra_sdk_env = pre_base_env.Clone(
    tools = ['naclsdk'],
    BUILD_TYPE = 'nacl_extra_sdk',
    BUILD_TYPE_DESCRIPTION = 'NaCl SDK extra library build',
    NACL_BUILD_FAMILY = 'UNTRUSTED',
    CPPPATH = ['${SOURCE_ROOT}'],
    CPPDEFINES = [
      ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
      ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
      ['NACL_BLOCK_SHIFT', '5' ],
      ['DYNAMIC_ANNOTATIONS_ENABLED', '1' ],
      ['DYNAMIC_ANNOTATIONS_PREFIX', 'NACL_' ],
      # This ensures that UINT32_MAX gets defined.
      ['__STDC_LIMIT_MACROS', '1'],
      # This ensures that PRId64 etc. get defined.
      ['__STDC_FORMAT_MACROS', '1'],
      ],
    ARFLAGS = 'rc'
)

if nacl_extra_sdk_env.Bit('bitcode'):
  target_root = nacl_extra_sdk_env.subst('${TARGET_ROOT}') + '-pnacl'
  nacl_extra_sdk_env.Replace(TARGET_ROOT=target_root)

# TODO(robertm): consider moving some of these flags to the naclsdk tool
nacl_extra_sdk_env.Append(CCFLAGS=['-Wall',
                                   '-fdiagnostics-show-option',
                                   '-pedantic'] +
                                  werror_flags +
                                  ['${EXTRA_CCFLAGS}',
                               ])

# Optionally disable certain link warning which cause ASMs to
# be injected into the object files
# This undoes the effect of src/untrusted/nosys/warning.h
if nacl_extra_sdk_env.Bit('disable_nosys_linker_warnings'):
  nacl_extra_sdk_env.Append(
      CPPDEFINES=['NATIVE_CLIENT_SRC_UNTRUSTED_NOSYS_WARNING_H_=1',
                  'stub_warning(n)=struct xyzzy',
                  'link_warning(n,m)=struct xyzzy',
                  ])

# TODO(robertm): remove this work-around for an llvm debug info bug
# http://code.google.com/p/nativeclient/issues/detail?id=235
if nacl_extra_sdk_env.Bit('target_arm'):
  nacl_extra_sdk_env.FilterOut(CCFLAGS=['-g'])

# TODO(robertm): remove this ASAP, we currently have llvm issue with c++
#                llvm-gcc is based on gcc 4.2 and has warning bug related
#                class constructors
if nacl_extra_sdk_env.Bit('target_arm'):
  nacl_extra_sdk_env.FilterOut(CCFLAGS = ['-Werror'])
  nacl_extra_sdk_env.Append(CFLAGS = werror_flags)

if nacl_extra_sdk_env.Bit('host_windows'):
  # NOTE: This is needed because Windows builds are case-insensitive.
  # Without this we use nacl-as, which doesn't handle include directives, etc.
  nacl_extra_sdk_env.Replace(ASCOM = '${CCCOM}')

if nacl_env.Bit('host_windows'):
  # NOTE: This is needed because Windows builds are case-insensitive.
  # Without this we use nacl-as, which doesn't handle include directives, etc.
  nacl_env.Replace(ASCOM = '${CCCOM}')

# ---------------------------------------------------------------------
# Special environment for untrusted test binaries that use raw syscalls
# ---------------------------------------------------------------------
def RawSyscallObjects(env, sources):
  raw_syscall_env = env.Clone()
  raw_syscall_env.Append(
    CPPDEFINES = [
      ['USE_RAW_SYSCALLS', '1'],
      ['NACL_BLOCK_SHIFT', '5'],
      ['NACL_BLOCK_SIZE', '32'],
      ['NACL_BUILD_ARCH', '${BUILD_ARCHITECTURE}' ],
      ['NACL_BUILD_SUBARCH', '${BUILD_SUBARCH}' ],
      ['NACL_TARGET_ARCH', '${TARGET_ARCHITECTURE}' ],
      ['NACL_TARGET_SUBARCH', '${TARGET_SUBARCH}' ],
      ],
  )
  objects = []
  for source_file in sources:
    target_name = 'raw_' + os.path.basename(source_file).rstrip('.c')
    object = raw_syscall_env.ComponentObject(target_name,
                                             source_file)
    objects.append(object)
  return objects

nacl_env.AddMethod(RawSyscallObjects)

# TODO(khim): document this
if not nacl_extra_sdk_env.Bit('nocpp'):
  nacl_extra_sdk_env.Append(
      BUILD_SCONSCRIPTS = [
        ####  ALPHABETICALLY SORTED ####
        'src/shared/imc/nacl.scons',
        'src/shared/npruntime/nacl.scons',
        'src/shared/platform/nacl.scons',
        'src/shared/ppapi/nacl.scons',
        'src/shared/ppapi_proxy/nacl.scons',
        'src/untrusted/gpu/nacl.scons',
        ####  ALPHABETICALLY SORTED ####
      ],
  )

if not nacl_extra_sdk_env.Bit('nacl_glibc'):
  # These are all specific to nacl-newlib so we do not include them
  # when building against nacl-glibc.  The functionality of
  # pthread/startup/stubs/nosys is provided by glibc.  The valgrind
  # code currently assumes nc_threads.
  nacl_extra_sdk_env.Append(
      BUILD_SCONSCRIPTS = [
        ####  ALPHABETICALLY SORTED ####
        'src/untrusted/pthread/nacl.scons',
        'src/untrusted/startup/nacl.scons',
        'src/untrusted/stubs/nacl.scons',
        'src/untrusted/nosys/nacl.scons',
        'src/untrusted/valgrind/nacl.scons',
        ####  ALPHABETICALLY SORTED ####
        ])

nacl_extra_sdk_env.Append(
    BUILD_SCONSCRIPTS = [
      ####  ALPHABETICALLY SORTED ####
      'src/include/nacl/nacl.scons',
      'src/shared/gio/nacl.scons',
      'src/shared/srpc/nacl.scons',
      'src/untrusted/crt/nacl.scons',
      'src/untrusted/av/nacl.scons',
      'src/untrusted/nacl/nacl.scons',
      ####  ALPHABETICALLY SORTED ####
   ],
)


environment_list.append(nacl_extra_sdk_env)

# ----------------------------------------------------------
# Targets for updating sdk headers and libraries
# NACL_SDK_XXX vars are defined by  site_scons/site_tools/naclsdk.py
# NOTE: our task here is complicated by the fact that there might already
#       some (outdated) headers/libraries at the new location
#       One of the hacks we employ here is to make every library depend
#       on the installation on ALL headers (sdk_headers)

# Contains all the headers to be installed
sdk_headers = nacl_extra_sdk_env.Alias('extra_sdk_update_header', [])
# Contains all the libraries and .o files to be installed
libs_platform = nacl_extra_sdk_env.Alias('extra_sdk_libs_platform', [])
libs = nacl_extra_sdk_env.Alias('extra_sdk_libs', [])
nacl_extra_sdk_env.Alias('extra_sdk_update', [libs, libs_platform])


# Add a header file to the toolchain.  By default, Native Client-specific
# headers go under nacl/, but there are non-specific headers, such as
# the OpenGLES2 headers, that go under their own subdir.
def AddHeaderToSdk(env, nodes, subdir = 'nacl/'):
  dir = ARGUMENTS.get('extra_sdk_include_destination')
  if not dir:
    dir = '${NACL_SDK_INCLUDE}'

  n = env.Replicate(dir + '/' + subdir, nodes)
  env.Alias('extra_sdk_update_header', n)
  return n

nacl_extra_sdk_env.AddMethod(AddHeaderToSdk)


def AddLibraryToSdkHelper(env, nodes, is_lib, is_platform):
  """"Helper function to install libs/objs into the toolchain
  and associate the action with the extra_sdk_update.

  Args:
    env: Environment in which we were called.
    nodes: list of libc/objs
    is_lib: treat nodes as libs
    is_platform: nodes are truly platform specific
  """
  # NOTE: hack see comment
  nacl_extra_sdk_env.Requires(nodes, sdk_headers)

  dir = ARGUMENTS.get('extra_sdk_lib_destination')
  if not dir:
    dir = '${NACL_SDK_LIB}/'

  if is_lib:
    n = env.ReplicatePublished(dir, nodes, 'link')
  else:
    n = env.Replicate(dir, nodes)

  if is_platform:
    env.Alias('extra_sdk_libs_platform', n)
  else:
    env.Alias('extra_sdk_libs', n)
  return n


def AddLibraryToSdk(env, nodes, is_platform=False):
  return AddLibraryToSdkHelper(env, nodes, True, is_platform)

nacl_extra_sdk_env.AddMethod(AddLibraryToSdk)


def AddObjectToSdk(env, nodes, is_platform=False):
    return AddLibraryToSdkHelper(env, nodes, False, is_platform)


nacl_extra_sdk_env.AddMethod(AddObjectToSdk)

# NOTE: a helpful target to test the sdk_extra magic
#       combine this with a 'MODE=nacl -c'
# TODO: the cleanup of libs is not complete
nacl_extra_sdk_env.Command('extra_sdk_clean', [],
                           ['rm -rf ${NACL_SDK_INCLUDE}/nacl*',
                            'rm -rf ${NACL_SDK_LIB}/libgoogle*',
                            'rm -rf ${NACL_SDK_LIB_PLATFORM}/crt[1in].*'])

# ----------------------------------------------------------
# CODE COVERAGE
# ----------------------------------------------------------
DeclareBit('coverage_enabled', 'The build should be instrumented to generate'
           'coverage information')

windows_coverage_env = windows_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-win',
    BUILD_TYPE_DESCRIPTION = 'Windows code coverage build',
    # TODO(bradnelson): switch nacl to common testing process so this won't be
    #    needed.
    MANIFEST_FILE = None,
    COVERAGE_ANALYZER_DIR=r'..\third_party\coverage_analyzer\bin',
    COVERAGE_ANALYZER='$COVERAGE_ANALYZER_DIR\coverage_analyzer.exe',
)
# TODO(bradnelson): switch nacl to common testing process so this won't be
#    needed.
windows_coverage_env['LINKCOM'] = windows_coverage_env.Action([
    windows_coverage_env.get('LINKCOM', []),
    '$COVERAGE_VSINSTR /COVERAGE ${TARGET}'])
windows_coverage_env.FilterOut(BUILD_GROUPS = ['default'])
windows_coverage_env.Append(LINKFLAGS = ['/NODEFAULTLIB:msvcrt'])
AddDualLibrary(windows_coverage_env)
environment_list.append(windows_coverage_env)

mac_coverage_env = mac_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-mac',
    BUILD_TYPE_DESCRIPTION = 'MacOS code coverage build',
    # Strict doesnt't currently work for coverage because the path to gcov is
    # magically baked into the compiler.
    LIBS_STRICT = False,
)
mac_coverage_env.FilterOut(BUILD_GROUPS = ['default'])
AddDualLibrary(mac_coverage_env)
environment_list.append(mac_coverage_env)

linux_coverage_env = linux_debug_env.Clone(
    tools = ['code_coverage'],
    BUILD_TYPE = 'coverage-linux',
    BUILD_TYPE_DESCRIPTION = 'Linux code coverage build',
    # Strict doesnt't currently work for coverage because the path to gcov is
    # magically baked into the compiler.
    LIBS_STRICT = False,
)
linux_coverage_env.FilterOut(BUILD_GROUPS = ['default'])
linux_coverage_env['OPTIONAL_COVERAGE_LIBS'] = '$COVERAGE_LIBS'
AddDualLibrary(linux_coverage_env)
environment_list.append(linux_coverage_env)

# ----------------------------------------------------------
# VARIOUS HELPERS
# ----------------------------------------------------------

doc_env = pre_base_env.Clone(
  NACL_BUILD_FAMILY = 'NO_PLATFORM',
  BUILD_TYPE = 'doc',
  BUILD_TYPE_DESCRIPTION = 'Documentation build',
  HOST_PLATFORMS = '*',
  HOST_PLATFORM_SUFFIX='',
)
environment_list.append(doc_env)
doc_env.Append(
  BUILD_SCONSCRIPTS = [
    'documentation',
    ],
)

# ----------------------------------------------------------
# Environment Massaging
# ----------------------------------------------------------
RELEVANT_CONFIG = ['NACL_BUILD_FAMILY',
                   'BUILD_TYPE',
                   'TARGET_ROOT',
                   'OBJ_ROOT',
                   'BUILD_TYPE_DESCRIPTION',
                   ]

MAYBE_RELEVANT_CONFIG = ['BUILD_OS',
                         'BUILD_ARCHITECTURE',
                         'BUILD_SUBARCH',
                         'TARGET_OS',
                         'TARGET_ARCHITECTURE',
                         'TARGET_SUBARCH',
                         'EMULATOR',
                         ]

def DumpCompilerVersion(cc, env):
  if 'gcc' in cc:
    env.Execute(env.Action('set'))
    env.Execute(env.Action('${CC} --v -c'))
    env.Execute(env.Action('${CC} -print-search-dirs'))
    env.Execute(env.Action('${CC} -print-libgcc-file-name'))
  elif cc.startswith('cl'):
    import subprocess
    try:
      p = subprocess.Popen(env.subst('${CC} /V'),
                           bufsize=1000*1000,
                           stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE)
    except WindowsError:
      print 'ERROR: Visual Studio tools are not in your path.'
      print '       Please run vcvars32.bat'
      print '       Typically located in:'
      print '          c:\\Program Files\\Microsoft Visual Studio 8\\VC\\Bin'
      sys.exit(1)
    stderr = p.stderr.read()
    stdout = p.stdout.read()
    retcode = p.wait()
    print stderr[0:stderr.find("\r")]
  else:
    print "UNKNOWN COMPILER"


def SanityCheckAndMapExtraction(all_envs, selected_envs):
  # simple completeness check
  for env in all_envs:
    for tag in RELEVANT_CONFIG:
      assert tag in env
      assert env[tag]

  # collect build families and enforce now dup rules
  family_map = {}
  for env in selected_envs:
    family = env['NACL_BUILD_FAMILY']
    if family not in family_map:
      family_map[family] = env
    else:
      print '\n\n\n\n'
      print 'You are using incompatible environments simultaneously'
      print
      print '%s vs %s' % (env['BUILD_TYPE'],
                         family_map[family]['BUILD_TYPE'])
      print """
      Please specfy the exact environments you require, e.g.
      MODE=dbg-linux,nacl

      """
      assert 0

  if VerboseConfigInfo(pre_base_env):
    Banner("The following environments have been configured")
    for family, env in family_map.iteritems():
      for tag in RELEVANT_CONFIG:
        assert tag in env
        print "%s:  %s" % (tag, env.subst(env.get(tag)))
      for tag in MAYBE_RELEVANT_CONFIG:
        print "%s:  %s" % (tag, env.subst(env.get(tag)))
      cc = env.subst('${CC}')
      print 'CC:', cc
      asppcom = env.subst('${ASPPCOM}')
      print 'ASPPCOM:', asppcom
      DumpCompilerVersion(cc, env)
      print
    rev_file = 'toolchain/linux_arm-untrusted/REV'
    if os.path.exists(rev_file):
      for line in open(rev_file).read().split('\n'):
        if "Revision:" in line:
          print "PNACL : %s" % line



  if 'TRUSTED' not in family_map or 'UNTRUSTED' not in family_map:
    Banner('W A R N I N G:')
    print "Not all families have envs this may cause some tests to not run"
  return family_map

# This is where TRUSTED_ENV is set.
def ExportSpecialFamilyVars(selected_envs, family_map):
    for family, family_env in family_map.iteritems():
      for env in selected_envs:
        env[family + '_ENV'] = family_env

# ----------------------------------------------------------
# Blank out defaults.
Default(None)


# Apply optional supplement if present in the directory tree.
if os.path.exists(pre_base_env.subst('$MAIN_DIR/supplement/supplement.scons')):
  SConscript('supplement/supplement.scons',
      exports=['environment_list', 'linux_env'])

# print sytem info (optionally)
if VerboseConfigInfo(pre_base_env):
  Banner('SCONS ARGS:' + str(sys.argv))
  os.system(pre_base_env.subst('${PYTHON} tools/sysinfo.py'))

CheckArguments()

selected_envs = FilterEnvironments(environment_list)
family_map = SanityCheckAndMapExtraction(environment_list, selected_envs)
ExportSpecialFamilyVars(selected_envs, family_map)

# Note: BuildEnvironments calls FilterEnvironments internally.
# Passing environment_list is OK
BuildEnvironments(environment_list)

# Change default to build everything, but not run tests.
Default(['all_programs', 'all_bundles', 'all_test_programs', 'all_libraries'])

# separate warnings from actual build output
Banner('B U I L D - O U T P U T:')
