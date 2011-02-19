.POSIX:

.SUFFIXES: .c .m .o

VERSION = 4

#CC=clang

CFLAGS += -std=c99 -fPIC
CPPFLAGS += -DTYPE_DEPENDENT_DISPATCH -DGNUSTEP
CPPFLAGS += -D__OBJC_RUNTIME_INTERNAL__=1 -D_XOPEN_SOURCE=500

PREFIX?= /usr/local
LIB_DIR= ${PREFIX}/lib
HEADER_DIR= ${PREFIX}/include

OBJECTS = \
	NSBlocks.o\
	Protocol2.o\
	abi_version.o\
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
	sync.o

all: libobjc.so.$(VERSION) libobjc.a

libobjc.so.$(VERSION): $(OBJECTS)
	@echo Linking shared library...
	@ld -shared -o $@ $(OBJECTS)

libobjc.a: $(OBJECTS)
	@echo Linking static library...
	@ld -r -s -o $@ $(OBJECTS)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.m.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: all
	install -m 444 libobjc.so.$(VERSION) $(LIB_DIR)
	install -m 444 libobjc.a $(LIB_DIR)
	ln -sf $(LIB_DIR)/libobjc.so.$(VERSION) $(LIB_DIR)/libobjc.so
	install -d $(HEADER_DIR)/objc
	install -m 444 objc/*.h $(HEADER_DIR)/objc

clean:
	rm -f $(OBJECTS)
	rm -f libobjc.so.$(VERSION)
	rm -f libobjc.a
