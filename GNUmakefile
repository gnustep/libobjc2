include $(GNUSTEP_MAKEFILES)/common.make

LIBRARY_NAME = libobjc 

libobjc_VERSION = 4

libobjc_OBJC_FILES = \
	blocks_runtime.m\
	linking.m\
	mutation.m\
	NXConstStr.m\
	Object.m\
	Protocol.m\
	sync.m


libobjc_C_FILES = \
	archive.c\
	class.c\
	encoding.c\
	exception.c\
	gc.c\
	hash.c\
	init.c\
	misc.c\
	nil_method.c\
	objects.c\
	protocol.c\
	runtime.c\
	sarray.c\
	selector.c\
	sendmsg.c\
	thr.c

# Deprecated functions are only deprecated for external use, not for us because
# we are special, precious, little flowers.
libobjc_CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1
# Note to Riccardo.  Please do not 'fix' C99isms in this.  The new ABI is only
# useful on compilers that support C99 (currently only clang), so there is no
# benefit from supporting platforms with no C99 compiler.
libobjc_CFLAGS += -Werror -std=c99 -g -fexceptions
libobjc_OBJCFLAGS += -g
libobjc_LDFLAGS += -g

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

ifneq ($(findstring gcc, $(CC)),)
libobjc_CFLAGS += -fgnu89-inline 
endif

ifneq ($(findstring mingw, $(GNUSTEP_HOST_OS)),)
libobjc_C_FILES += libobjc_entry.c
endif

include $(GNUSTEP_MAKEFILES)/library.make
