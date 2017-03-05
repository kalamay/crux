# The default build is probably ok for most things, but it may be customized
# a bit. This can be done by specifying definitions for the following
# configuration options. These values should either be specified on the make
# command line and/or placed in a file named "Custom.mk" next to the Makefile.
#
# Build Configuration Options:
#   PREFIX: install prefix for shared libraries (default "/usr/local")
#   DESTDIR: install target directory (default "")
#
#   BUILD: either "release" (default) or "debug"
#   BUILD_ROOT: directory to build in (default "build")
#
#   EXECINFO: 1 or 0 to override enabling or disabling execinfo
#   EXECINFO_H: location of execinfo.h
#   EXECINFO_LIB: location of the execinfo library
#   LDFLAGS_EXECINFO: override linker flags for execinfo
#   
#   FLAGS_COMMON: common compiler and linker flags for release and debug builds
#   CFLAGS_COMMON: compiler flags for release and debug builds
#   CFLAGS_DEBUG: debug only compiler flags
#   CFLAGS_RELEASE: release only compiler flags
#   CFLAGS: override for final compiler flags
#
#   LDFLAGS_COMMON: linker flags for release and debug builds
#   LDFLAGS_DEBUG: debug only linker flags
#   LDFLAGS_RELEASE: release only linker flags
#   LDFLAGS: override for final linker flags

-include Custom.mk

PREFIX?=/usr/local
DESTDIR?=
VERSION_MAJOR:= 0
VERSION_MINOR:= 1
VERSION_PATCH:= 0
VERSION:=$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
VERSION_COMPAT:=$(VERSION_MAJOR).$(VERSION_MINOR)
VERSION_DEF_MAJOR:=\#define XVERSION_MAJOR $(VERSION_MAJOR)
VERSION_DEF_MINOR:=\#define XVERSION_MINOR $(VERSION_MINOR)
VERSION_DEF_PATCH:=\#define XVERSION_PATCH $(VERSION_PATCH)

# detect execinfo and update build flags
EXECINFO_H?= $(wildcard /usr/include/execinfo.h /usr/local/include/execinfo.h)
ifeq ($(EXECINFO_H),)
 EXECINFO?=0
else
 EXECINFO?=1
 EXECINFO_LIB?=$(wildcard $(EXECINFO_H:%/include/execinfo.h=%/lib/libexecinfo.so))
 ifneq ($(EXECINFO_LIB),)
  LDFLAGS_EXECINFO?= -lexecinfo
 endif
endif

# set default compiler and linker flags
FLAGS_COMMON?= -march=native
CFLAGS_COMMON?= $(FLAGS_COMMON) \
	-DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH) \
	-DHAS_EXECINFO=$(EXECINFO) -std=gnu11 -fPIC
CFLAGS_DEBUG?= $(CFLAGS_COMMON) -g -Wall -Wextra -Wcast-align -pedantic -Werror
CFLAGS_RELEASE?= $(CFLAGS_COMMON) -O2 -DNDEBUG -flto
LDFLAGS_COMMON?= $(FLAGS_COMMON) $(LDFLAGS_EXECINFO)
LDFLAGS_DEBUG?= $(LDFLAGS_COMMON)
LDFLAGS_RELEASE?= $(LDFLAGS_COMMON)

# define static and dynamic library names and set platform library flags
LIB:= libcrux.a
ifeq ($(shell uname),Darwin)
  SO:=libcrux.$(VERSION).dylib
  SO_COMPAT:=libcrux.$(VERSION_COMPAT).dylib
  SO_ANY:=libcrux.dylib
  SOFLAGS:= -dynamiclib -install_name $(PREFIX)/lib/$(SO) -compatibility_version $(VERSION_COMPAT) -current_version $(VERSION)
else
  SO:=libcrux.so.$(VERSION)
  SO_COMPAT:=libcrux.so.$(VERSION_MAJOR)
  SO_ANY:=libcrux.so
  SOFLAGS:= -shared -Wl,-rpath=$(PREFIX)/lib
endif

# directory segment for man pages
MANDIR:= share/man/man3

# set build directory variables
BUILD?= release
BUILD_ROOT?= build
BUILD_UPPER:= $(shell echo $(BUILD) | tr a-z A-Z)
BUILD_TYPE:= $(BUILD_ROOT)/$(BUILD)
BUILD_LIB:= $(BUILD_TYPE)/lib
BUILD_INCLUDE:= $(BUILD_TYPE)/include
BUILD_MAN:= $(BUILD_TYPE)/$(MANDIR)
BUILD_TMP:= $(BUILD_ROOT)/tmp/$(BUILD)

# update final build flags
CFLAGS?= $(CFLAGS_$(BUILD_UPPER))
CFLAGS:= $(CFLAGS) -fno-omit-frame-pointer -MMD -MP -Iinclude -I$(BUILD_TMP)
LDFLAGS?= $(LDFLAGS_$(BUILD_UPPER))

# list of souce files to include in build
SRC:= \
	src/err.c \
	src/base.c \
	src/heap.c \
	src/task.c \
	src/poll.c \
	src/hub.c \
	src/hash.c \
	src/http.c

# list of header files to include in build
INCLUDE:= \
	include/crux.h \
	include/crux/clock.h \
	include/crux/ctx.h \
	include/crux/def.h \
	include/crux/err.h \
	include/crux/heap.h \
	include/crux/hub.h \
	include/crux/list.h \
	include/crux/poll.h \
	include/crux/task.h \
	include/crux/version.h \
	include/crux/rand.h \
	include/crux/seed.h \
	include/crux/vec.h \
	include/crux/hash.h \
	include/crux/hashtier.h \
	include/crux/hashmap.h

# list of manual pages
MAN:= \
	man/crux-task.3 \
	man/crux-hub.3

# list of source files for testing
TEST:= \
	test/err.c \
	test/clock.c \
	test/vec.c \
	test/hash.c \
	test/hashmap.c \
	test/hashtier.c \
	test/heap.c \
	test/hub.c \
	test/poll.c \
	test/task.c

# list of files to install
INSTALL:= \
	$(DESTDIR)$(PREFIX)/lib/$(SO) \
	$(DESTDIR)$(PREFIX)/lib/$(SO_COMPAT) \
	$(DESTDIR)$(PREFIX)/lib/$(SO_ANY) \
	$(DESTDIR)$(PREFIX)/lib/$(LIB) \
	$(INCLUDE:%=$(DESTDIR)$(PREFIX)/%) \
	$(MAN:man/%=$(DESTDIR)$(PREFIX)/$(MANDIR)/%)

# object files mapped from source files
SRC_OBJ:= $(SRC:src/%.c=$(BUILD_TMP)/crux-%.o)
# object files mapped from test files
TEST_OBJ:= $(TEST:test/%.c=$(BUILD_TMP)/crux-test-%.o)
# executable files mapped from test files
TEST_BIN:= $(TEST:test/%.c=$(BUILD_TMP)/test-%)
# build header files mapped from include files
INCLUDE_OUT:=$(INCLUDE:%=$(BUILD_TYPE)/%)
# build man pages mapped from man source files
MAN_OUT:=$(MAN:man/%=$(BUILD_MAN)/%)

all: static dynamic include man

# compile and run all tests
test: $(TEST_BIN)
	@for t in $^; do ./$$t; done

# create static library
static: $(BUILD_LIB)/$(LIB)

# compile and link shared library
dynamic: $(BUILD_LIB)/$(SO) $(BUILD_LIB)/$(SO_COMPAT) $(BUILD_LIB)/$(SO_ANY)

# copy all header files into build directory
include: $(INCLUDE_OUT)

# copy all header files into build directory
man: $(MAN_OUT)

# install all build files into destination
install: $(INSTALL)

# copy file from build directory into install destination
$(DESTDIR)$(PREFIX)/%: $(BUILD_TYPE)/%
	@mkdir -p $(dir $@)
	cp -R $< $@

# remove installed files for the current version
uninstall:
	rm -f $(INSTALL)
	@if [ -d $(DESTDIR)$(PREFIX)/include/crux ]; then rmdir $(DESTDIR)$(PREFIX)/include/crux; fi

# build and execute test
test-%: $(BUILD_TMP)/test-%
	./$<

# set config.h as a dependency for all source files
$(SRC): $(BUILD_TMP)/config.h

# generate config.h
$(BUILD_TMP)/config.h: bin/config.py | $(BUILD_TMP)
	python $< > $@

# create static library archive
$(BUILD_LIB)/$(LIB): $(SRC_OBJ) | $(BUILD_LIB)
	$(AR) rcus $@ $^

# link shared library
$(BUILD_LIB)/$(SO): $(SRC_OBJ) | $(BUILD_LIB)
	$(CC) $(LDFLAGS) $(SOFLAGS) $^ -o $@

# create symbolic link for shared library
$(BUILD_LIB)/$(SO_COMPAT) $(BUILD_LIB)/$(SO_ANY):
	cd $(BUILD_LIB) && ln -s $(SO) $(notdir $@)

# copy source headers into build directory
$(BUILD_INCLUDE)/%: include/% | $(BUILD_INCLUDE)/crux
	cp $< $@

# override rule for version.h
$(BUILD_INCLUDE)/crux/version.h: include/crux/version.h Makefile | $(BUILD_INCLUDE)/crux
	awk 'NR==4{print "$(VERSION_DEF_MAJOR)\n$(VERSION_DEF_MINOR)\n$(VERSION_DEF_PATCH)\n"}1' $< > $@

# copy source headers into build directory
$(BUILD_MAN)/%: man/% | $(BUILD_MAN)
	cp $< $@

# link test executables
$(BUILD_TMP)/test-%: $(BUILD_TMP)/crux-test-%.o $(SRC_OBJ) | $(BUILD_TMP)
	$(CC) $(LDFLAGS) $^ -o $@

# compile main object files
$(BUILD_TMP)/crux-%.o: src/%.c Makefile | $(BUILD_TMP)
	$(CC) $(CFLAGS) -c $< -o $@

# compile test object files
$(BUILD_TMP)/crux-test-%.o: test/%.c Makefile | $(BUILD_TMP)
	$(CC) $(CFLAGS) -c $< -o $@

# create directory paths
$(BUILD_LIB) $(BUILD_INCLUDE)/crux $(BUILD_MAN) $(BUILD_TMP):
	mkdir -p $@

# removes the build directory
clean:
	rm -rf $(BUILD_ROOT)

.PHONY: all test static dynamic include man install uninstall clean
.PRECIOUS: $(SRC_OBJ) $(TEST_OBJ) $(INCLUDE_OUT) $(MAN_OUT)

# include compiler-build dependency files
-include $(SRC_OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)

