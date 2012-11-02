 
# Check to see if GNUstep-config is available.
ifeq ($(GNUSTEP_MAKEFILES),)
GNUSTEP_MAKEFILES := $(shell gnustep-config --variable=GNUSTEP_MAKEFILES 2>/dev/null)
endif

ifeq ($(GNUSTEP_MAKEFILES),)
#
# Start of section for building without GNUstep
#
$(warning GNUstep not found -\
building for standalone installation.)

include Makefile

#
# End of section for building without GNUstep-make
#
else
#
# Start of GNUstep specific section.
#
$(warning GNUstep found -\
building for install in the GNUstep filesystem.)

PACKAGE_NAME = gnustep-objc2
SVN_MODULE_NAME = libobjc2
SVN_BASE_URL = svn+ssh://svn.gna.org/svn/gnustep/libs
SVN_TAG_NAME=objc2

GNUSTEP_INSTALLATION_DOMAIN ?= LOCAL

ifeq ($(messages),yes)
  SILENT ?= 
endif

INSTALL := dummy_install

include Makefile

LIB_DIR := $(shell gnustep-config --variable=GNUSTEP_$(GNUSTEP_INSTALLATION_DOMAIN)_LIBRARIES 2>/dev/null)
ifeq ($(LIB_DIR),)
LIB_DIR := $(shell gmake -s -f "$(GNUSTEP_MAKEFILES)/empty.make" print-gnustep-install-libraries GNUSTEP_INSTALLATION_DOMAIN=$(GNUSTEP_INSTALLATION_DOMAIN) quiet=yes 2>/dev/null)
endif
ifeq ($(LIB_DIR),)
$(error Unable to use gnustep-config to get install directory - is gnustep-config in your PATH?)
endif

HEADER_DIR := $(shell gnustep-config --variable=GNUSTEP_$(GNUSTEP_INSTALLATION_DOMAIN)_HEADERS 2>/dev/null)
ifeq ($(HEADER_DIR),)
HEADER_DIR := $(shell gmake -s -f "$(GNUSTEP_MAKEFILES)/empty.make" print-gnustep-install-headers GNUSTEP_INSTALLATION_DOMAIN=$(GNUSTEP_INSTALLATION_DOMAIN) quiet=yes 2>/dev/null)
endif
ifeq ($(HEADER_DIR),)
$(error Unable to use gnustep-config to get install directory - is gnustep-config in your PATH?)
endif

install: all
	$(SILENT)echo Installing libraries...
	$(SILENT)install -d $(LIB_DIR)
	$(SILENT)install -m 444 $(STRIP) $(LIBOBJC).so.$(VERSION) $(LIB_DIR)
	$(SILENT)install -m 444 $(STRIP) $(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)
	$(SILENT)install -m 444 $(STRIP) $(LIBOBJC).a $(LIB_DIR)
	$(SILENT)echo Creating symbolic links...
	$(SILENT)ln -sf $(LIBOBJC).so.$(VERSION) $(LIB_DIR)/$(LIBOBJC).so
	$(SILENT)ln -sf $(LIBOBJC).so.$(VERSION) $(LIB_DIR)/$(LIBOBJC).so.$(MAJOR_VERSION)
	$(SILENT)ln -sf $(LIBOBJC).so.$(VERSION) $(LIB_DIR)/$(LIBOBJC).so.$(MAJOR_VERSION).$(MINOR_VERSION)
	$(SILENT)ln -sf $(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)/$(LIBOBJCXX).so
	$(SILENT)ln -sf $(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)/$(LIBOBJCXX).so.$(MAJOR_VERSION)
	$(SILENT)ln -sf $(LIBOBJCXX).so.$(VERSION) $(LIB_DIR)/$(LIBOBJCXX).so.$(MAJOR_VERSION).$(MINOR_VERSION)
	$(SILENT)echo Installing headers...
	$(SILENT)install -d $(HEADER_DIR)/objc
	$(SILENT)install -m 444 objc/*.h $(HEADER_DIR)/objc
	$(SILENT)install -m 444 objc/*.h $(HEADER_DIR)/objc
	$(SILENT)echo "To use the newly installed objc2 library with GNUstep,"
	$(SILENT)echo "please change to your gnustep-make directory and"
	$(SILENT)echo "type 'configure; make install' to tell gnustep-make"
	$(SILENT)echo "to use the new library (you will then need to configure" 
	$(SILENT)echo "and build gnustep-base to use the new library too)." 

distclean: clean

#
# End of GNUstep-make specific section.
#
endif
