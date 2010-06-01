include $(GNUSTEP_MAKEFILES)/common.make

LIBRARY_NAME = libobjc 

SUBPROJECTS = toydispatch

libobjc_VERSION = 4

libobjc_OBJC_FILES = \
	NSBlocks.m\
	NXConstStr.m\
	Object.m\
	Protocol.m\
	blocks_runtime.m\
	linking.m\
	mutation.m\
	properties.m\
	sync.m


libobjc_C_FILES = \
	class_table.c\
	encoding.c\
	hash_table.c\
	exception.c\
	hooks.c\
	ivar.c\
	init.c\
	misc.c\
	nil_method.c\
	protocol.c\
	runtime.c\
	sarray2.c\
	selector_table.c\
	sendmsg.c

ifneq ($(enable_legacy), no)
libobjc_C_FILES += \
	gc.c\
	objects.c\
	thr.c
endif

libobjc_HEADER_FILES_DIR = objc
libobjc_HEADER_FILES_INSTALL_DIR = objc
ifneq ($(install_headers), no)
libobjc_HEADER_FILES = \
	Availability.h\
	NXConstStr.h\
	Object.h\
	Protocol.h\
	blocks_runtime.h\
	encoding.h\
	hash.h\
	objc-api.h\
	objc-decls.h\
	objc-list.h\
	objc.h\
	runtime-legacy.h\
	runtime.h\
	sarray.h\
	slot.h\
	thr.h\
	typedstream.h
endif

libobjc_LIBRARIES_DEPEND_UPON += -lpthread

# Deprecated functions are only deprecated for external use, not for us because
# we are special, precious, little flowers.
libobjc_CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1 -D_XOPEN_SOURCE=500
# Note to Riccardo.  Please do not 'fix' C99isms in this.  The new ABI is only
# useful on compilers that support C99 (currently only clang), so there is no
# benefit from supporting platforms with no C99 compiler.
libobjc_CFLAGS += -Werror -std=c99 -g -march=native -fexceptions #-fno-inline
libobjc_OBJCFLAGS += -g -std=c99 -march=native
libobjc_LDFLAGS += -g -ltoydispatch
libobjc_LIB_DIRS += -L toydispatch/obj

libobjc_CFLAGS +=  -O3

ifneq ($(findstring gcc, $(CC)),)
libobjc_CFLAGS += -fgnu89-inline 
endif

ifneq ($(findstring mingw, $(GNUSTEP_HOST_OS)),)
libobjc_C_FILES += libobjc_entry.c
endif

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/library.make

ifeq ($(findstring no, $(debug)),)
before-all::
	@echo
	@echo
	@echo WARNING: You are building in debug mode.  This will generate a LOT of console \
	output for every Objective-C program you run.  If this is not what you \
	want, please compile with $(MAKE) debug=no
	@echo
	@echo
endif

