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
#   By default, all features are compiled. To cherry pick features, define
#   any of these values as 0. Some features have dependencies and will be
#   implicitly disabled if the dependencies are not enabled:
#
#   WITH_POLL: include the poller
#   WITH_TASK: include coroutines
#   WITH_HUB: include the hub (requires poll and task)
#   WITH_HTTP: include the http parser
#   WITH_NET: include the network system (requires hub)
#   WITH_DNS: include the dns parser and data structures
#   WITH_RESOLV: include the dns resolver (requires hub, net, and dns)
#   WITH_READLINE: include the async readline (requires hub)
#
#   EXECINFO: 1 or 0 to override enabling or disabling execinfo
#   EXECINFO_H: location of execinfo.h
#   EXECINFO_LIB: location of the execinfo library
#   LDFLAGS_EXECINFO: override linker flags for execinfo
#   
#   FLAGS_common: common compiler and linker flags for release and debug builds
#   CFLAGS_common: compiler flags for release and debug builds
#   CFLAGS_debug: debug only compiler flags
#   CFLAGS_release: release only compiler flags
#   CFLAGS: override for final compiler flags
#
#   LDFLAGS_common: linker flags for release and debug builds
#   LDFLAGS_debug: debug only linker flags
#   LDFLAGS_release: release only linker flags
#   LDFLAGS: override for final linker flags

-include Custom.mk

WITH_POLL?=1
WITH_TASK?=1
WITH_HUB?=1
WITH_HTTP?=1
WITH_NET?=1
WITH_DNS?=1
WITH_RESOLV?=1
WITH_READLINE?=1
ifneq ($(WITH_POLL)$(WITH_TASK),11)
 WITH_HUB:=0
endif
ifneq ($(WITH_HUB),1)
 WITH_NET:=0
endif
ifneq ($(WITH_HUB)$(WITH_DNS)$(WITH_NET),111)
 WITH_RESOLV:=0
endif
ifneq ($(WITH_HUB),1)
 WITH_READLINE:=0
endif

WITH:= $(WITH_POLL)$(WITH_TASK)$(WITH_HUB)$(WITH_HTTP)$(WITH_NET)$(WITH_DNS)$(WITH_RESOLV)$(WITH_READLINE)

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
FLAGS_common?= -march=native
CFLAGS_common?= $(FLAGS_common) \
	-DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH) \
	-std=gnu11 -fPIC
CFLAGS_debug?= $(CFLAGS_common) -g -Wall -Wextra -Wcast-align -pedantic -Werror -fno-omit-frame-pointer -fsanitize=address
CFLAGS_release?= $(CFLAGS_common) -O3 -DNDEBUG
LDFLAGS_common?= $(FLAGS_common) $(LDFLAGS_EXECINFO)
LDFLAGS_debug?= $(LDFLAGS_common) -fsanitize=address
LDFLAGS_release?= $(LDFLAGS_common) -O3

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
BUILD_TYPE:= $(BUILD_ROOT)/$(BUILD)
BUILD_LIB:= $(BUILD_TYPE)/lib
BUILD_INCLUDE:= $(BUILD_TYPE)/include
BUILD_MAN:= $(BUILD_TYPE)/$(MANDIR)
BUILD_TMP:= $(BUILD_ROOT)/tmp/$(BUILD)

# update final build flags
CFLAGS?= $(CFLAGS_$(BUILD))
CFLAGS:= $(CFLAGS) -fno-omit-frame-pointer -MMD -MP -Iinclude -I$(BUILD_TMP)
LDFLAGS?= $(LDFLAGS_$(BUILD))

# list of souce files to include in build
SRC:= \
	src/err.c \
	src/base.c \
	src/map.c \
	src/heap.c \
	src/hash.c \
	src/vm.c \
	src/buf.c

# list of header files to include in build
INCLUDE:= \
	include/crux.h \
	include/crux/version.h \
	include/crux/clock.h \
	include/crux/range.h \
	include/crux/value.h \
	include/crux/def.h \
	include/crux/err.h \
	include/crux/rand.h \
	include/crux/seed.h \
	include/crux/vm.h \
	include/crux/buf.h \
	include/crux/list.h \
	include/crux/vec.h \
	include/crux/heap.h \
	include/crux/hash.h \
	include/crux/hashtier.h \
	include/crux/hashmap.h \
	include/crux/map.h


# list of manual pages
MAN:=

# list of source files for testing
TEST:= \
	test/err.c \
	test/clock.c \
	test/vec.c \
	test/hash.c \
	test/hashtier.c \
	test/hashmap.c \
	test/map.c \
	test/heap.c \
	test/buf.c \
	test/http.c

ifeq ($(WITH_POLL),1)
 SRC+= src/poll.c
 INCLUDE+= include/crux/poll.h
 TEST+= test/poll.c
endif
ifeq ($(WITH_TASK),1)
 SRC+= src/task.c
 INCLUDE+= include/crux/ctx.h include/crux/task.h
 MAN+= man/crux-task.3
 TEST+= test/task.c
endif
ifeq ($(WITH_HUB),1)
 SRC+= src/hub.c
 INCLUDE+= include/crux/hub.h
 MAN+= man/crux-hub.3
 TEST+= test/hub.c
endif
ifeq ($(WITH_HTTP),1)
 SRC+= src/http.c
 INCLUDE+= include/crux/http.h
endif
ifeq ($(WITH_NET),1)
 SRC+= src/net.c
 INCLUDE+= include/crux/net.h
endif
ifeq ($(WITH_DNS),1)
 SRC+= src/dns.c src/dnsc.c
 INCLUDE+= include/crux/dns.h include/crux/dnsc.h
 TEST+= test/dns.c
endif
ifeq ($(WITH_RESOLV),1)
 SRC+= src/resolv.c
 INCLUDE+= include/crux/resolv.h
 TEST+= test/resolv.c
endif
ifeq ($(WITH_READLINE),1)
 SRC+= src/readline.c
 INCLUDE+= include/crux/readline.h
endif

# list of files to install
INSTALL:= \
	$(DESTDIR)$(PREFIX)/lib/$(SO) \
	$(DESTDIR)$(PREFIX)/lib/$(SO_COMPAT) \
	$(DESTDIR)$(PREFIX)/lib/$(SO_ANY) \
	$(DESTDIR)$(PREFIX)/lib/$(LIB) \
	$(INCLUDE:%=$(DESTDIR)$(PREFIX)/%) \
	$(MAN:man/%=$(DESTDIR)$(PREFIX)/$(MANDIR)/%.gz)

# object files mapped from source files
SRC_OBJ:= $(SRC:src/%.c=$(BUILD_TMP)/crux-%.o)
# object files mapped from test files
TEST_OBJ:= $(TEST:test/%.c=$(BUILD_TMP)/crux-test-%.o)
# executable files mapped from test files
TEST_BIN:= $(TEST:test/%.c=$(BUILD_TMP)/test-%)
# build header files mapped from include files
INCLUDE_OUT:=$(INCLUDE:%=$(BUILD_TYPE)/%)
# build man pages mapped from man source files
MAN_OUT:=$(MAN:man/%=$(BUILD_MAN)/%.gz)

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
$(BUILD_TMP)/config.h: bin/config.py Makefile | $(BUILD_TMP)
	python $< > $@
	@echo "#ifndef HAS_EXECINFO" >> $@
	@echo "# define HAS_EXECINFO $(EXECINFO)" >> $@
	@echo "#endif" >> $@
	@echo "#define WITH_POLL $(WITH_POLL)" >> $@
	@echo "#define WITH_TASK $(WITH_TASK)" >> $@
	@echo "#define WITH_HUB $(WITH_HUB)" >> $@
	@echo "#define WITH_HTTP $(WITH_HTTP)" >> $@
	@echo "#define WITH_NET $(WITH_NET)" >> $@
	@echo "#define WITH_DNS $(WITH_DNS)" >> $@
	@echo "#define WITH_RESOLV $(WITH_RESOLV)" >> $@
	@echo "#define WITH_READLINE $(WITH_READLINE)" >> $@

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

# copy source headers into build directory
$(BUILD_MAN)/%.gz: man/% | $(BUILD_MAN)
	gzip < $< > $@

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

show-files:
	@echo "SRC: $(SRC)\n" | tr ' ' '\n'
	@echo "INCLUDE: $(INCLUDE)\n" | tr ' ' '\n'
	@echo "TEST: $(TEST)\n" | tr ' ' '\n'
	@echo "MAN: $(MAN)\n" | tr ' ' '\n'

# removes the build directory
clean:
	rm -rf $(BUILD_ROOT)

.PHONY: all test static dynamic include man install uninstall show-files clean
.PRECIOUS: $(SRC_OBJ) $(TEST_OBJ) $(INCLUDE_OUT) $(MAN_OUT)

# include compiler-build dependency files
-include $(SRC_OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)

