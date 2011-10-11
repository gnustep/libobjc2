.POSIX:

.SUFFIXES: .cc .c .m .o .S

MAJOR_VERSION = 4
MINOR_VERSION = 6
SUBMINOR_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION).$(SUBMINOR_VERSION)

LIBOBJCLIBNAME=objc
LIBOBJC=libobjc
LIBOBJCXX=libobjcxx

CFLAGS += -std=gnu99 -fPIC -fexceptions
CXXFLAGS += -fPIC -fexceptions
CPPFLAGS += -DTYPE_DEPENDENT_DISPATCH -DGNUSTEP
CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1 -D_XOPEN_SOURCE=500 -D__BSD_VISIBLE=1

# Suppress warnings about incorrect selectors
CPPFLAGS += -DNO_SELECTOR_MISMATCH_WARNINGS
# Some helpful flags for debugging.
CPPFLAGS += -g -O0 -fno-inline

PREFIX?= /usr/local
LIB_DIR= ${PREFIX}/lib
HEADER_DIR= ${PREFIX}/include

OBJCXX_OBJECTS = \
	objcxx_eh.o

OBJECTS = \
	NSBlocks.o\
	Protocol2.o\
	abi_version.o\
	alias_table.o\
	arc.o\
	associate.o\
	blocks_runtime.o\
	block_to_imp.o\
	block_trampolines.o\
	caps.o\
	category_loader.o\
	class_table.o\
	dtable.o\
	eh_personality.o\
	encoding2.o\
	gc_none.o\
	hash_table.o\
	hooks.o\
	ivar.o\
	legacy_malloc.o\
	loader.o\
	mutation.o\
	properties.o\
	protocol.o\
	runtime.o\
	sarray2.o\
	selector_table.o\
	sendmsg2.o\
	statics_loader.o\
	toydispatch.o

all: $(LIBOBJC).a $(LIBOBJCXX).so.$(VERSION)

$(LIBOBJCXX).so.$(VERSION): $(LIBOBJC).so.$(VERSION) $(OBJCXX_OBJECTS)
	@echo Linking shared Objective-C++ runtime library...
	@$(CXX) -shared -o $@ $(OBJCXX_OBJECTS)

$(LIBOBJC).so.$(VERSION): $(OBJECTS)
	@echo Linking shared Objective-C runtime library...
	@$(CC) -shared -rdynamic -o $@ $(OBJECTS)

$(LIBOBJC).a: $(OBJECTS)
	@echo Linking static Objective-C runtime library...
	@ld -r -s -o $@ $(OBJECTS)

.cc.o: Makefile
	@echo Compiling `basename $<`...
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.c.o: Makefile
	@echo Compiling `basename $<`...
	@$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.m.o: Makefile
	@echo Compiling `basename $<`...
	@$(CC) $(CPPFLAGS) $(CFLAGS) -fobjc-exceptions -c $< -o $@

.S.o: Makefile
	@echo Assembling `basename $<`...
	@$(CC) $(CPPFLAGS) -c $< -o $@

install: all
	@echo Installing libraries...
	@install -d $(LIB_DIR)
	@install -m 444 $(LIBOBJC).so.$(VERSION) $(LIB_DIR)
	@install -m 444 $(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)
	@install -m 444 $(LIBOBJC).a $(LIB_DIR)
	@echo Creating symbolic links...
	@ln -sf $(LIB_DIR)/$(LIBOBJC).so.$(VERSION) $(LIB_DIR)/$(LIBOBJC).so
	@ln -sf $(LIB_DIR)/$(LIBOBJC).so.$(VERSION) $(LIB_DIR)/$(LIBOBJC).so.$(MAJOR_VERSION)
	@ln -sf $(LIB_DIR)/$(LIBOBJC).so.$(VERSION) $(LIB_DIR)/$(LIBOBJC).so.$(MAJOR_VERSION).$(MINOR_VERSION)
	@ln -sf $(LIB_DIR)/$(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)/$(LIBOBJCXX).so
	@ln -sf $(LIB_DIR)/$(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)/$(LIBOBJCXX).so.$(MAJOR_VERSION)
	@ln -sf $(LIB_DIR)/$(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)/$(LIBOBJCXX).so.$(MAJOR_VERSION).$(MINOR_VERSION)
	@echo Installing headers...
	@install -d $(HEADER_DIR)/objc
	@install -m 444 objc/*.h $(HEADER_DIR)/objc

clean:
	@echo Cleaning...
	@rm -f $(OBJECTS)
	@rm -f $(OBJCXX_OBJECTS)
	@rm -f $(LIBOBJC).so.$(VERSION)
	@rm -f $(LIBOBJCXX).so.$(VERSION)
	@rm -f $(LIBOBJC).a
