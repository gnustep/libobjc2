.POSIX:

VERSION = 4

#CC=clang

CFLAGS += -std=c99
CPPFLAGS += -DTYPE_DEPENDENT_DISPATCH

#LIB_DIR=/usr/local/GNUstep/Local/Library/Libraries/
#HEADER_DIR=/usr/local/GNUstep/Local/Library/Headers
LIB_DIR=/tmp/usr/lib/
HEADER_DIR=/tmp/usr/include/

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

all: libobjc.so.$(VERSION)

libobjc.so.$(VERSION): $(OBJECTS)
	@echo Linking shared library...
	ld -shared -o $@ $(OBJECTS)

.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

.m.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

install: all
	install -m 444 libobjc.so.$(VERSION) $(LIB_DIR)
	ln -sf $(LIB_DIR)/libobjc.so.$(VERSION) $(LIB_DIR)/libobjc.so
	install -d $(HEADER_DIR)/objc
	install -m 444 objc/*.h $(HEADER_DIR)/objc

clean:
	rm -f $(OBJECTS)
	rm -f libobjc.so.$(VERSION)
