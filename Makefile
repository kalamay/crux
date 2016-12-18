# The default build is probably ok for most things, but it may be customized
# a bit. This can be done by specifying definitions for the following
# configuration options. These values should either be specified on the make
# command line and/or placed in a file named "Custom.mk" next to the Makefile.
#
# Build Configuration Options:
#   EXECINFO: 1 or 0 to override enabling or disabling execinfo
#   EXECINFO_H: location of execinfo.h
#   EXECINFO_LIB: location of the execinfo library
#   LDFLAGS_EXECINFO: override linker flags for execinfo
#
#   BUILD: either "release" (default) or "debug"
#   BUILD_ROOT: directory to build in (default "build")
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
CFLAGS_COMMON?= $(FLAGS_COMMON) -DHAS_EXECINFO=$(EXECINFO) -std=gnu11
CFLAGS_DEBUG?= $(CFLAGS_COMMON) -g -Wall -Wextra -Wcast-align -pedantic -Werror
CFLAGS_RELEASE?= $(CFLAGS_COMMON) -O3 -DNDEBUG -flto
LDFLAGS_COMMON?= $(FLAGS_COMMON) $(LDFLAGS_EXECINFO)
LDFLAGS_DEBUG?= $(LDFLAGS_COMMON)
LDFLAGS_RELEASE?= $(LDFLAGS_COMMON)

BUILD?= release
BUILD_ROOT?= build
BUILD_UPPER:= $(shell echo $(BUILD) | tr a-z A-Z)
BUILD_TYPE:= $(BUILD_ROOT)/$(BUILD)
BUILD_BIN:= $(BUILD_TYPE)/bin
BUILD_TEST:= $(BUILD_TYPE)/test
BUILD_OBJ:= $(BUILD_TYPE)/obj
BUILD_TMP:= $(BUILD_TYPE)/tmp

CFLAGS?= $(CFLAGS_$(BUILD_UPPER))
CFLAGS:= $(CFLAGS) -fno-omit-frame-pointer -MMD -MP -Iinclude -I$(BUILD_TMP)
LDFLAGS?= $(LDFLAGS_$(BUILD_UPPER))

SRC:= src/err.c src/heap.c src/clock.c src/task.c src/poll.c src/hub.c
TEST:= test/heap.c test/clock.c test/task.c test/poll.c test/hub.c
SRC_OBJ:= $(SRC:src/%.c=$(BUILD_OBJ)/crux-%.o)
TEST_OBJ:= $(TEST:test/%.c=$(BUILD_OBJ)/crux-test-%.o)
TEST_BIN:= $(TEST:test/%.c=$(BUILD_TEST)/%)

test: $(TEST_BIN)
	@for t in $^; do ./$$t; done

test-%: $(BUILD_TEST)/%
	./$<

$(SRC): $(BUILD_TMP)/config.h

$(BUILD_TMP)/config.h: bin/config.py | $(BUILD_TMP)
	python $< > $@

$(BUILD_TEST)/%: $(BUILD_OBJ)/crux-test-%.o $(SRC_OBJ) | $(BUILD_TEST)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILD_OBJ)/crux-%.o: src/%.c Makefile | $(BUILD_OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_OBJ)/crux-test-%.o: test/%.c Makefile | $(BUILD_OBJ)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_BIN) $(BUILD_TEST) $(BUILD_OBJ) $(BUILD_TMP):
	mkdir -p $@

clean:
	rm -rf $(BUILD_ROOT)

.PHONY: all test clean
.PRECIOUS: $(SRC_OBJ) $(TEST_OBJ)

-include $(SRC_OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)

