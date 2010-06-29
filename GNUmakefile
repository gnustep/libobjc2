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
	category_loader.c\
	class_table.c\
	dtable.c\
	encoding2.c\
	hash_table.c\
	eh_personality.c\
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
libobjc_C_FILES += \
	legacy_malloc.c\
	objects.c\
	thr.c

endif

libobjc_HEADER_FILES_DIR = objc
libobjc_HEADER_FILES_INSTALL_DIR = objc
ifneq ($(install_headers), no)
libobjc_HEADER_FILES = \
	Availability.h\
	blocks_runtime.h\
	runtime.h\
	slot.h
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

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/library.make
