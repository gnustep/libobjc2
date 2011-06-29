
PACKAGE_NAME = gnustep-objc2
VERSION=1.4.0
SVN_MODULE_NAME = libobjc2
SVN_BASE_URL = svn+ssh://svn.gna.org/svn/gnustep/libs
SVN_TAG_NAME=objc2

include $(GNUSTEP_MAKEFILES)/common.make

LIBOBJC = libobjc
LIBOBJCLIBNAME = objc
LIBOBJCXX = libobjcxx

LIBRARY_NAME = ${LIBOBJC} ${LIBOBJCXX}

${LIBOBJC}_VERSION = 4

${LIBOBJC}_OBJC_FILES = \
	NSBlocks.m\
	Protocol2.m\
	arc.m\
	associate.m\
	blocks_runtime.m\
	mutation.m\
	properties.m


${LIBOBJC}_C_FILES = \
	abi_version.c\
	alias_table.c\
	caps.c\
	category_loader.c\
	class_table.c\
	dtable.c\
	eh_personality.c\
	encoding2.c\
	gc_none.c\
	hash_table.c\
	hooks.c\
	ivar.c\
	loader.c\
	protocol.c\
	runtime.c\
	sarray2.c\
	selector_table.c\
	sendmsg2.c\
	statics_loader.c\
	toydispatch.c

${LIBOBJCXX}_CC_FILES = objcxx_eh.cc
${LIBOBJCXX}_LDFLAGS = -L./obj/$(GNUSTEP_TARGET_LDIR)/ -lstdc++ -l${LIBOBJCLIBNAME}

ifeq ($(disable_legacy), yes)
${LIBOBJC}_CPPFLAGS += -DNO_LEGACY
else
${LIBOBJC}_C_FILES += legacy_malloc.c
endif

${LIBOBJC}_HEADER_FILES_DIR = objc
${LIBOBJC}_HEADER_FILES_INSTALL_DIR = objc
ifneq ($(install_headers), no)
${LIBOBJC}_HEADER_FILES = \
	Availability.h\
	Object.h\
	Protocol.h\
	blocks_runtime.h\
	capabilities.h\
	encoding.h\
	hooks.h\
	runtime.h\
	runtime-deprecated.h\
	slot.h\
	objc.h\
	objc-api.h\
	objc-arc.h\
	objc-auto.h\
	toydispatch.h
endif

# Disable type dependent dispatch if tdd=no is specified
ifneq ($(tdd), no)
${LIBOBJC}_CPPFLAGS += -DTYPE_DEPENDENT_DISPATCH
endif

ifeq ($(low_memory), yes)
${LIBOBJC}_CPPFLAGS += -D__OBJC_LOW_MEMORY__
endif

ifneq ($(boehm_gc), no)
${LIBOBJC}_C_FILES += gc_boehm.c
ifneq ($(findstring linux, $(GNUSTEP_TARGET_OS)), linux)
${LIBOBJC}_LIBRARIES_DEPEND_UPON += -lgc-threaded -lexecinfo
else
${LIBOBJC}_LIBRARIES_DEPEND_UPON += -lgc
endif
#${LIBOBJC}_OBJCFLAGS += -fobjc-gc
${LIBOBJC}_CPPFLAGS += -DENABLE_GC
endif



ifeq ($(findstring openbsd, $(GNUSTEP_HOST_OS)), openbsd)
${LIBOBJC}_LIBRARIES_DEPEND_UPON += -pthread 
else
${LIBOBJC}_LIBRARIES_DEPEND_UPON += -lpthread 
endif

# If we're doing a release build, don't tell people that the code that they're
# using is rubbish - they complain.
#CPPFLAGS += -DNO_SELECTOR_MISMATCH_WARNINGS

${LIBOBJC}_CFLAGS +=  -O3

# Deprecated functions are only deprecated for external use, not for us because
# we are special, precious, little flowers.
${LIBOBJC}_CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1 -D_XOPEN_SOURCE=500
# Note to Riccardo.  Please do not 'fix' C99isms in this.  The new ABI is only
# useful on compilers that support C99 (currently only clang), so there is no
# benefit from supporting platforms with no C99 compiler.
${LIBOBJC}_CFLAGS += -std=gnu99 -g -fexceptions #-fvisibility=hidden
${LIBOBJC}_CCFLAGS += -std=c++98 -g -fexceptions #-fvisibility=hidden
${LIBOBJC}_CFLAGS += -Wno-unused-function

# Uncomment this when debugging - it makes everything slow, but means that the
# debugger actually works...
ifeq ($(debug), yes)
${LIBOBJC}_CFLAGS += -fno-inline
${LIBOBJC}_CPPFLAGS += -DGC_DEBUG
endif
${LIBOBJC}_OBJCFLAGS += $(${LIBOBJC}_CFLAGS) $(${LIBOBJC}_CFLAGS)

ifneq ($(findstring gcc, $(CC)),)
  # Hack to get the __sync_* GCC builtins to work with GCC
  ifeq ($(GNUSTEP_TARGET_CPU), ix86)
    ${LIBOBJC}_CFLAGS += -march=i586
  endif
endif

ifeq ($(GNUSTEP_TARGET_OS), mingw32)
# Hack to get mingw to provide declaration for strdup (since it is non-standard)
${LIBOBJC}_CPPFLAGS += -U__STRICT_ANSI__
endif

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/library.make

build-opts:
	@echo Building LLVM optimisation passes...
	@sh build_opts.sh $(MAKE) all

install-opts: build_opts
	@echo Installing LLVM optimisation passes...
	@sh build_opts.sh $(MAKE) install

clean-opts:
	@echo Cleaning LLVM optimisation passes...
	@sh build_opts.sh $(MAKE) clean
