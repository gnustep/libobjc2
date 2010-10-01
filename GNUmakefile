
PACKAGE_NAME = gnustep-objc2
VERSION=1.0.0
SVN_MODULE_NAME = libobjc2
SVN_BASE_URL = svn+ssh://svn.gna.org/svn/gnustep/libs
SVN_TAG_NAME=objc2

include $(GNUSTEP_MAKEFILES)/common.make

LIBRARY_NAME = libobjc


SUBPROJECTS = toydispatch

libobjc_VERSION = 4

libobjc_OBJC_FILES = \
	NSBlocks.m\
	Protocol2.m\
	blocks_runtime.m\
	mutation.m\
	properties.m\
	sync.m


libobjc_C_FILES = \
	abi_version.c\
	caps.c\
	category_loader.c\
	class_table.c\
	dtable.c\
	eh_personality.c\
	encoding2.c\
	hash_table.c\
	hooks.c\
	ivar.c\
	loader.c\
	protocol.c\
	runtime.c\
	sarray2.c\
	selector_table.c\
	sendmsg2.c\
	statics_loader.c

ifneq ($(enable_legacy), no)
libobjc_C_FILES += legacy_malloc.c
endif

libobjc_HEADER_FILES_DIR = objc
libobjc_HEADER_FILES_INSTALL_DIR = objc
ifneq ($(install_headers), no)
libobjc_HEADER_FILES = \
	Availability.h\
	Object.h\
	Protocol.h\
	blocks_runtime.h\
	capabilities.h\
	encoding.h\
	hooks.h\
	runtime.h\
	slot.h\
	objc.h\
	objc-api.h
endif

ifneq ($(tdd), no)
libobjc_CPPFLAGS += -DTYPE_DEPENDENT_DISPATCH
endif

libobjc_LIBRARIES_DEPEND_UPON += -lpthread -ltoydispatch

# Deprecated functions are only deprecated for external use, not for us because
# we are special, precious, little flowers.
libobjc_CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1 -D_XOPEN_SOURCE=500
# Note to Riccardo.  Please do not 'fix' C99isms in this.  The new ABI is only
# useful on compilers that support C99 (currently only clang), so there is no
# benefit from supporting platforms with no C99 compiler.
libobjc_CFLAGS += -std=c99 -g -fexceptions -fno-inline
libobjc_OBJCFLAGS += $(libobjc_CFLAGS)
libobjc_LDFLAGS += -g
libobjc_LIB_DIRS += -L toydispatch/obj

libobjc_CFLAGS +=  -O3

ifneq ($(findstring gcc, $(CC)),)
  # Hack to get the __sync_* GCC builtins to work with GCC
  ifeq ($(GNUSTEP_TARGET_CPU), ix86)
    libobjc_CFLAGS += -march=i586
  endif
endif

ifeq ($(GNUSTEP_TARGET_OS), mingw32)
# Hack to get mingw to provide declaration for strdup (since it is non-standard)
libobjc_CPPFLAGS += -U__STRICT_ANSI__
endif

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/library.make
