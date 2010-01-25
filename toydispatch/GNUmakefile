include $(GNUSTEP_MAKEFILES)/common.make

LIBRARY_NAME = toydispatch

toydispatch_VERSION = 1

toydispatch_C_FILES = \
	toydispatch.c

toydispatch_HEADER_FILES_INSTALL_DIR = toydispatch
toydispatch_HEADER_FILES = \
	toydispatch.h

toydispatch_LIBRARIES_DEPEND_UPON += -lpthread

toydispatch_CFLAGS += -Werror -std=c99 -march=native

include $(GNUSTEP_MAKEFILES)/library.make
