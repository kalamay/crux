# The default build is probably ok for most things, but it may be customized
# a bit. This can be done by specifying definitions for the following
# configuration options. These values should either be specified on the make
# command line and/or placed in a file named "Custom.mk" next to the Makefile.
#
# Build Configuration Options:
#   NAME: name of the target (default "crux")
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

NAME?=crux
PREFIX?=/usr/local
DESTDIR?=
VERSION_MAJOR:= 0
VERSION_MINOR:= 1
VERSION_PATCH:= 0
VERSION:=$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_PATCH)
VERSION_COMPAT:=$(VERSION_MAJOR).$(VERSION_MINOR)

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

FLAGS_COMMON?= -march=native
CFLAGS_COMMON?= $(FLAGS_COMMON) \
	-DVERSION_MAJOR=$(VERSION_MAJOR) -DVERSION_MINOR=$(VERSION_MINOR) -DVERSION_PATCH=$(VERSION_PATCH) \
	-DHAS_EXECINFO=$(EXECINFO) -std=gnu11 -fPIC
CFLAGS_DEBUG?= $(CFLAGS_COMMON) -g -Wall -Wextra -Wcast-align -pedantic -Werror
CFLAGS_RELEASE?= $(CFLAGS_COMMON) -Os -DNDEBUG -flto
LDFLAGS_COMMON?= $(FLAGS_COMMON) $(LDFLAGS_EXECINFO)
LDFLAGS_DEBUG?= $(LDFLAGS_COMMON)
LDFLAGS_RELEASE?= $(LDFLAGS_COMMON)

LIB:= lib$(NAME).a
ifeq ($(shell uname),Darwin)
  SO:=lib$(NAME).$(VERSION).dylib
  SO_COMPAT:=lib$(NAME).$(VERSION_COMPAT).dylib
  SO_ANY:=lib$(NAME).dylib
  SOFLAGS:= -dynamiclib -install_name $(PREFIX)/lib/$(SO) -compatibility_version $(VERSION_COMPAT) -current_version $(VERSION)
else
  SO:=lib$(NAME).so.$(VERSION)
  SO_COMPAT:=lib$(NAME).so.$(VERSION_MAJOR)
  SO_ANY:=lib$(NAME).so
  SOFLAGS:= -shared -Wl,-rpath=$(PREFIX)/lib
endif

BUILD?= release
BUILD_ROOT?= build
BUILD_UPPER:= $(shell echo $(BUILD) | tr a-z A-Z)
BUILD_TYPE:= $(BUILD_ROOT)/$(BUILD)
BUILD_BIN:= $(BUILD_TYPE)/bin
BUILD_LIB:= $(BUILD_TYPE)/lib
BUILD_TEST:= $(BUILD_TYPE)/test
BUILD_OBJ:= $(BUILD_TYPE)/obj
BUILD_TMP:= $(BUILD_TYPE)/tmp

CFLAGS?= $(CFLAGS_$(BUILD_UPPER))
CFLAGS:= $(CFLAGS) -fno-omit-frame-pointer -MMD -MP -Iinclude -I$(BUILD_TMP)
LDFLAGS?= $(LDFLAGS_$(BUILD_UPPER))

SRC:= src/version.c src/err.c src/heap.c src/clock.c src/task.c src/poll.c src/hub.c
TEST:= test/heap.c test/clock.c test/task.c test/poll.c test/hub.c
SRC_OBJ:= $(SRC:src/%.c=$(BUILD_OBJ)/$(NAME)-%.o)
TEST_OBJ:= $(TEST:test/%.c=$(BUILD_OBJ)/$(NAME)-test-%.o)
TEST_BIN:= $(TEST:test/%.c=$(BUILD_TEST)/%)

all: static dynamic

test: $(TEST_BIN)
	@for t in $^; do ./$$t; done

static: $(BUILD_LIB)/$(LIB)

dynamic: $(BUILD_LIB)/$(SO) $(BUILD_LIB)/$(SO_COMPAT) $(BUILD_LIB)/$(SO_ANY)

install: $(BUILD_LIB)/$(LIB) $(BUILD_LIB)/$(SO) $(BUILD_LIB)/$(SO_COMPAT) $(BUILD_LIB)/$(SO_ANY)
	mkdir -p $(DESTDIR)$(PREFIX)/lib
	mkdir -p $(DESTDIR)$(PREFIX)/include/crux
	cp $(BUILD_LIB)/$(LIB) $(BUILD_LIB)/$(SO) $(BUILD_LIB)/$(SO_COMPAT) $(BUILD_LIB)/$(SO_ANY) $(DESTDIR)$(PREFIX)/lib
	cp include/crux.h $(DESTDIR)$(PREFIX)/include
	cp include/crux/*.h $(DESTDIR)$(PREFIX)/include/crux

uninstall:
	rm -r \
		$(DESTDIR)$(PREFIX)/include/crux \
		$(DESTDIR)$(PREFIX)/include/crux.h \
		$(DESTDIR)$(PREFIX)/lib/$(LIB) \
		$(DESTDIR)$(PREFIX)/lib/$(SO) \
		$(DESTDIR)$(PREFIX)/lib/$(SO_COMPAT) \
		$(DESTDIR)$(PREFIX)/lib/$(SO_ANY)

test-%: $(BUILD_TEST)/%
	./$<

$(SRC): $(BUILD_TMP)/config.h

$(BUILD_TMP)/config.h: bin/config.py | $(BUILD_TMP)
	python $< > $@

$(BUILD_LIB)/$(LIB): $(SRC_OBJ) | $(BUILD_LIB)
	$(AR) rcus $@ $^
			
$(BUILD_LIB)/$(SO): $(SRC_OBJ) | $(BUILD_LIB)
	$(CC) $(LDFLAGS) $(SOFLAGS) $^ -o $@

$(BUILD_LIB)/$(SO_COMPAT) $(BUILD_LIB)/$(SO_ANY): $(BUILD_LIB)/$(SO)
	cd $(BUILD_LIB) && ln -s $(SO) $(notdir $@)

$(BUILD_TEST)/%: $(BUILD_OBJ)/$(NAME)-test-%.o $(SRC_OBJ) | $(BUILD_TEST)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_OBJ)/$(NAME)-%.o: src/%.c Makefile | $(BUILD_OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_OBJ)/$(NAME)-test-%.o: test/%.c Makefile | $(BUILD_OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_BIN) $(BUILD_LIB) $(BUILD_TEST) $(BUILD_OBJ) $(BUILD_TMP):
	mkdir -p $@

clean:
	rm -rf $(BUILD_ROOT)

.PHONY: all test static dynamic install uninstall clean
.PRECIOUS: $(SRC_OBJ) $(TEST_OBJ)

-include $(SRC_OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)

