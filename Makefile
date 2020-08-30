level=0 # this variable can be overridden when the make command is run, e.g., 'make level=1'

CPP=g++ 
CPPFLAGS=-ggdb -Wall -D'TRACE_LEVEL=$(level)'
DEPFLAGS=-MT $@ -MMD -MP -MF $(DEPDIR)/$*.d # create dependency file when source file is compiled
DEPDIR=dep
OBJDIR=obj
BINDIR=bin
BIN=main

SRCS  = $(wildcard *.cpp)
OBJS  = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SRCS))
DEPS := $(SRCS:%.cpp=$(DEPDIR)/%.d)

.PHONY = all default prep build clean # list of targets/recipes that are not files

all: default

default: prep build
	@echo $(BINDIR)/$(BIN) is ready.

prep:
	@cp samples/disk* .
	@mkdir -p $(DEPDIR)
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)

build: $(BINDIR)/$(BIN)

clean:
	@echo "Removing all non-source files"
	@rm -f disk*
	@rm -f $(DEPDIR)/*
	@rm -rf $(OBJDIR)
	@rm -rf $(BINDIR)

$(BINDIR)/$(BIN) : $(OBJS) # the executable file depends on having up-to-date object files
	@echo "Linking" $(BINDIR)/$(BIN)
	@$(CPP) $(CPPFLAGS) $(OBJS) -o $(BINDIR)/$(BIN)

$(OBJDIR)/%.o : %.cpp $(DEPDIR)/%.d | $(DEPDIR) # object files depend on up-to-date source and related dependency files
	@echo "Compiling" $<
	@$(CPP) -c $(CPPFLAGS) $(DEPFLAGS) $< -o $@

$(DEPS):
include $(wildcard $(DEPS))
