CC := $(CXX)
CFLAGS := -pthread -O3 -DNDEBUG
STD := c++17
SRCEXT := cpp
SRCDIR := src
TESTDIR := unit_test
BUILDDIR := build/src
TESTBUILD := build/unit_test
MAIN_TARGET := ./bin/exec
TEST_TARGET := ./bin/test
SLIB_TARGET := ./lib/pomlib.a
INCLD := include

MAIN_SOURCES := $(shell find $(SRCDIR) -type f -name *.$(SRCEXT))
TEST_SOURCES := $(shell find $(TESTDIR) -type f -name *.$(SRCEXT))

SWITCH  := $(addprefix build/,$(MAIN_SOURCES:.cpp=.o))
TWITCH  := $(addprefix build/,$(TEST_SOURCES:.cpp=.o))

MAIN_OBJECTS := $(SWITCH)
MAIN_OBJS_NOMAIN := $(filter-out $(BUILDDIR)/main.o, $(MAIN_OBJECTS))
TEST_OBJECTS := $(TWITCH)

MODULE1 := bboard
MODULE2 := agents

INC := -I include/

all:    main test lib

lib : $(MAIN_OBJS_NOMAIN)
	@mkdir -p lib
	@ar rcs $(SLIB_TARGET) $^
	@echo "Building Library"
	@$(MAKE) main -s

main: $(MAIN_OBJECTS)
	@mkdir -p bin
	@$(CC) $(CFLAGS) -std=$(STD) $^ -o $(MAIN_TARGET)

test: $(TEST_OBJECTS)
	@$(MAKE) main -s
	@mkdir -p bin
	@$(CC) $(CFLAGS) -std=$(STD) $^ -o $(TEST_TARGET) $(MAIN_OBJS_NOMAIN)

# build main test files
build/$(TESTDIR)/%.o: $(TESTDIR)/%.$(SRCEXT)
	@echo "Building test"
	@mkdir -p $(TESTBUILD)
	@$(CC) $(CFLAGS) -std=$(STD) -c -o $@ $< $(INC) -I $(TESTDIR)

# build test files
build/$(TESTDIR)/$(MODULE1)/%.o: $(TESTDIR)/$(MODULE1)/%.$(SRCEXT)
	@echo "Building test-case: " $@
	@mkdir -p $(TESTBUILD) -p $(TESTBUILD)/$(MODULE1)
	@$(CC) $(CFLAGS) -std=$(STD) -c -o $@ $< $(INC) -I $(TESTDIR)

# build main files
build/src/%.o: $(SRCDIR)/%.$(SRCEXT)
	@echo "Building main: " $@
	@mkdir -p $(BUILDDIR)
	@$(CC) $(CFLAGS) -std=$(STD) -c -o $@ $< $(INC)
build/src/$(MODULE1)/%.o: src/$(MODULE1)/%.$(SRCEXT)
	@echo "Building bboard: " $@
	@mkdir -p $(BUILDDIR) -p $(BUILDDIR)/$(MODULE1)
	@$(CC) $(CFLAGS) -std=$(STD) -c -o $@ $< $(INC)
build/src/$(MODULE2)/%.o: src/$(MODULE2)/%.$(SRCEXT)
	@echo "Building agents: $@"
	@mkdir -p $(BUILDDIR) -p $(BUILDDIR)/$(MODULE2)
	@$(CC) $(CFLAGS) -std=$(STD) -c -o $@ $< $(INC)

clean:
	@echo " Cleaning..."; 
	@echo " $(RM) -r $(BUILDDIR) $(MAIN_TARGET) $(SLIB_TARGET)"; $(RM) -r $(BUILDDIR) $(MAIN_TARGET) $(SLIB_TARGET)
	@echo " Clean test files except test_main"; find $(TESTBUILD) $(TEST_TARGET) -type f -not -name 'test_main.o' -print0 | xargs -0 $(RM) --
	@echo
# only cleans main
mclean:
	@echo " Cleaning..."; 
	@echo " $(RM) -r $(BUILDDIR) $(MAIN_TARGET)"; $(RM) -r $(BUILDDIR) $(MAIN_TARGET)
	@echo
