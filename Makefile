.POSIX:

.SUFFIXES: .cc .c .m .o

MAJOR_VERSION = 1
MINOR_VERSION = 3
SUBMINOR_VERSION = 0
VERSION = $(MAJOR_VERSION).$(MINOR_VERSION).$(SUBMINOR_VERSION)

CFLAGS += -std=gnu99 -fPIC
CXXFLAGS += -fPIC
CPPFLAGS += -DTYPE_DEPENDENT_DISPATCH -DGNUSTEP
CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1 -D_XOPEN_SOURCE=500

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
	blocks_runtime.o\
	caps.o\
	category_loader.o\
	class_table.o\
	dtable.o\
	eh_personality.o\
	encoding2.o\
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
	sync.o\
	toydispatch.o

all: libobjc.so.$(VERSION) libobjc.a libobjcxx.so.$(VERSION)

libobjcxx.so.$(VERSION): $(OBJCXX_OBJECTS)
	@echo Linking shared library...
	@$(CXX) -Wl,-shared -o $@ $(OBJCXX_OBJECTS)

libobjc.so.$(VERSION): $(OBJECTS)
	@echo Linking shared library...
	@ld -shared -o $@ $(OBJECTS)

libobjc.a: $(OBJECTS)
	@echo Linking static library...
	@ld -Ur -s -o $@ $(OBJECTS)

.cc.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.m.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: all
	install -d $(LIB_DIR)
	install -m 444 libobjc.so.$(VERSION) $(LIB_DIR)
	install -m 444 libobjcxx.so.$(VERSION) $(LIB_DIR)
	install -m 444 libobjc.a $(LIB_DIR)
	ln -sf $(LIB_DIR)/libobjc.so.$(VERSION) $(LIB_DIR)/libobjc.so
	ln -sf $(LIB_DIR)/libobjc.so.$(VERSION) $(LIB_DIR)/libobjc.so.$(MAJOR_VERSION)
	ln -sf $(LIB_DIR)/libobjc.so.$(VERSION) $(LIB_DIR)/libobjc.so.$(MAJOR_VERSION).$(MINOR_VERSION)
	ln -sf $(LIB_DIR)/libobjcxx.so.$(VERSION) $(LIB_DIR)/libobjcxx.so
	ln -sf $(LIB_DIR)/libobjcxx.so.$(VERSION) $(LIB_DIR)/libobjcxx.so.$(MAJOR_VERSION)
	ln -sf $(LIB_DIR)/libobjcxx.so.$(VERSION) $(LIB_DIR)/libobjcxx.so.$(MAJOR_VERSION).$(MINOR_VERSION)
	install -d $(HEADER_DIR)/objc
	install -m 444 objc/*.h $(HEADER_DIR)/objc

clean:
	rm -f $(OBJECTS)
	rm -f libobjc.so.$(VERSION)
	rm -f libobjcxx.so.$(VERSION)
	rm -f libobjc.a
