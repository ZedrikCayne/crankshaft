CC:=gcc
GXX:=g++
INCDIRS:=include
SRCDIR=src
TESTDIR=test
AR:=ar

VERSION=0.1.0

#DEBUG=-g

OS=$(shell uname -o)

ifeq ($(OS),Darwin)
SSL_LOCATION=/opt/homebrew/Cellar/openssl@3/3.3.1
CFLAGS:=$(CFLAGS) -Wno-unknown-warning-option -Wno-unused-command-line-argument -I$(SSL_LOCATION)/include -L$(SSL_LOCATION)/lib
endif

CFILES=$(foreach D,$(SRCDIR),$(wildcard $(D)/*.c))
CXXFILES=$(foreach D,$(SRCDIR),$(wildcard $(D)/*.cpp))
TESTCFILES=$(foreach D,$(TESTDIR),$(wildcard $(D)/*.c))
TESTCXXFILES=$(foreach D,$(TESTDIR),$(wildcard $(D)/*.cpp))
CFLAGS:=-Wall $(DEBUG) $(OPT) $(foreach D,$(INCDIRS),-I$(D)) $(CFLAGS)
CXXFLAGS:=$(CFLAGS)
LIBS:=-lssl -lcrypto -lstdc++ -lpthread -lm -lz

OBJECTS_DIR=obj
LIB_DIR=lib
LIBOUT=$(LIB_DIR)/libcrankshaft.a
BUILD_DIR=build
TAROUT=$(BUILD_DIR)/crankshaft-$(VERSION).tgz


OBJECTS_C=$(patsubst %.c, $(OBJECTS_DIR)/%.o, $(notdir $(CFILES)))
OBJECTS_CXX=$(patsubst %.cpp, $(OBJECTS_DIR)/%.o, $(notdir $(CXXFILES)))
TESTOBJECTS_C=$(patsubst %.c, $(OBJECTS_DIR)/%.o, $(notdir $(TESTCFILES)))
TESTOBJECTS_CXX=$(patsubst %.cpp, $(OBJECTS_DIR)/%.o, $(notdir $(TESTCXXFILES)))

INCLUDES=$(foreach D,$(INCDIRS), $(shell find $(D) -type f -name "*"))

BINOUT=crankshaft

all: $(OBJECTS_DIR) $(BINOUT)

library: $(LIB_DIR) $(LIBOUT)

publish: $(BUILD_DIR) $(TAROUT)

clean:
	@rm -rvf $(BINOUT)* $(OBJECTS_DIR)/*.o $(LIB_DIR)/* $(BUILD_DIR)/*

run: all
	./$(BINOUT) --trace

debug: all
	gdb --args $(BINOUT) --trace

testWithMalloc: CFLAGS:=-DCS_ALLOC_USE_MALLOC $(CFLAGS)
testWithMalloc: test

test: CFLAGS:=-DCS_TEST_SKIP_TESTTEST $(CFLAGS)
test: CFLAGS:=-DCS_AUTOTEST_ENABLED $(CFLAGS)
test: $(OBJECTS_DIR) $(BINOUT)-test
	./$(BINOUT)-test --test --suppress-errors --only-fails

valgrind:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$(BINOUT)-test-valgrind.txt ./$(BINOUT)-test --test --suppress-errors --only-fails

valgrindServer:
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=$(BINOUT)-valgrind.txt ./$(BINOUT) --trace

$(LIB_DIR):
	mkdir -p $@

$(OBJECTS_DIR):
	mkdir -p $@

$(BUILD_DIR):
	mkdir -p $@

$(BINOUT)-test: $(OBJECTS_C) $(OBJECTS_CXX) $(TESTOBJECTS_C) $(TESTOBJECTS_CXX)
	$(CC) -o $@ $(CXXFLAGS) -Xlinker $^ ${LIBS}

$(TAROUT): $(BUILD_DIR) $(LIBOUT)
	tar -czf $@ $(LIB_DIR) $(INCDIRS)

$(LIBOUT): $(OBJECTS_C)
	$(AR) rcs $@ $(OBJECTS_C)

$(BINOUT): $(OBJECTS_C) $(OBJECTS_CXX)
	$(CC) -o $@ $(CXXFLAGS) -Xlinker $^ ${LIBS}

$(OBJECTS_DIR)/%.o: $(SRCDIR)/%.c $(INCLUDES)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJECTS_DIR)/%.o: $(SRCDIR)/%.cpp $(INCLUDES)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(OBJECTS_DIR)/%.o: $(TESTDIR)/%.c $(INCLUDES)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJECTS_DIR)/%.o: $(TESTDIR)/%.cpp $(INCLUDES)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: clean all debug test run lib

