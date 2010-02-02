include $(GNUSTEP_MAKEFILES)/common.make

#LIBRARY_NAME = GCKit
TOOL_NAME = GCKit

GCKit_VERSION = 1

GCKit_OBJC_FILES = \
				cycle.m\
				inline.m\
				malloc.m\
				thread.m\
				trace.m\
				test.m\
				visit.m\
				workqueue.m

GCKit_HEADER_FILES_INSTALL_DIR = GCKit

GCKit_HEADER_FILES = \
	GCKit.h

GCKit_LIBRARIES_DEPEND_UPON += -lpthread

GCKit_OBJCFLAGS += -Werror -std=c99 -march=native

include $(GNUSTEP_MAKEFILES)/library.make
include $(GNUSTEP_MAKEFILES)/tool.make
