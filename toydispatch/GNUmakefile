include $(GNUSTEP_MAKEFILES)/common.make

LIBRARY_NAME = toydispatch

toydispatch_VERSION = 1

toydispatch_C_FILES = \
	toydispatch.c

toydispatch_HEADER_FILES_INSTALL_DIR = toydispatch
toydispatch_HEADER_FILES = \
	toydispatch.h

toydispatch_LIBRARIES_DEPEND_UPON += -lpthread

toydispatch_CFLAGS += -Werror -std=c99

ifeq ($(GNUSTEP_TARGET_OS), mingw32)
# Hack to get the __sync_* GCC builtins to work on Windows
toydispatch_CFLAGS += -march=i586
# Hack to get mingw to provide declaration for strdup (since it is non-standard)
toydispatch_CPPFLAGS += -U__STRICT_ANSI__
endif

include $(GNUSTEP_MAKEFILES)/library.make
