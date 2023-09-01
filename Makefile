CC = gcc
CFLAGS = -Wall -Wextra

SHELLNAME = utcsh
CFLAGS_REL = -O2 -g
CFLAGS_DEB = -O0 -g3 -fno-omit-frame-pointer
CFLAGS_SAN = -fsanitize=undefined -fsanitize=address -fno-omit-frame-pointer -g

TESTDIR=tests
TESTSCRIPT=$(TESTDIR)/run-tests.py

SRCS = src/utcsh.c src/util.c 
SIGSRCS = src/mykill.c src/handle.c
HEADERS = src/util.h src/utcsh.r
SIGHEADERS = src/util.h src/utcsh.r

VPATH = src

FILES = $(SRCS) $(HEADERS)

$(SHELLNAME): $(FILES)
	$(CC) $(CFLAGS) $(CFLAGS_REL) $(SRCS) -o $(SHELLNAME)

debug: $(FILES)
	$(CC) $(CFLAGS) $(CFLAGS_DEB) $(SRCS) -o $(SHELLNAME)

asan: $(FILES)
	$(CC) $(CFLAGS) $(CFLAGS_SAN) $(SRCS) -o $(SHELLNAME)

##################################
# Settings for fib and utilities #
##################################

# We place these here instead of at the top because make's implicit rule
# (when run just as `make` instead of `make utcsh` or `make fib` is the first
# rule in the file, and we don't really want that to be fib or argprinter

fib: fib.c
	$(CC) $(CFLAGS) -o fib $<

handle: handle.o
	$(CC) $(CFLAGS) handle.o -o handle

mykill: mykill.o
	$(CC) $(CFLAGS) mykill.o -o mykill

argprinter: argprinter.c
	$(CC) $(CFLAGS) -o argprinter $<

################################
# Prepare your work for upload #
################################

FILENAME = turnin.tar

turnin.tar: clean
	tar cvf $(FILENAME) `find . -type f | grep -v \.git | grep -v \.tar$$ | grep -v \.tar\.gz$$ | grep -v \.swp$$ | grep -v ~$$`
	gzip $(FILENAME)

turnin: turnin.tar
	@echo "================="
	@echo "Created $(FILENAME).gz for submission.  Please upload to Canvas."
	@echo "Before uploading, please verify:"
	@echo "     - Your README is correctly filled out."
	@echo "     - Your pair programming log is in the project directory."
	@echo "If either of those items are not done, please update your submission and run the make turnin command again."
	@ls -al $(FILENAME).gz

#########################
# Various utility rules #
#########################

clean:
	rm -f $(SHELLNAME) *.o *~
	rm -f .utcsh.grade.json readme.html shellspec.html
	rm -f fib argprinter
	rm -rf tests-out

# Checks that the test scripts have valid executable permissions and fix them if not.
validtestperms: $(TESTSCRIPT)
	@test -x $(TESTSCRIPT)  && { echo "Testscript permissions appear correct!"; } || \
{ echo "Testscript does not have executable permissions. Please run \`chmod u+x $(TESTSCRIPT)\` and try again.";\
echo "Also verify that *all* files in \`tests/test-utils\` have executable permissions before continuing,";\
echo "or future tests could silently break."; exit 1; }

fixtestscriptperms:
	@chmod u+x $(TESTSCRIPT)
	@chmod u+x tests/test-utils/*
	@chmod u+x tests/test-utils/p2a-test/*

.PHONY: clean fixtestscriptperms

##############
# Test Cases #
##############

check: $(SHELLNAME) validtestperms
	@echo "Running all tests..."
	$(TESTSCRIPT) -kv

testcase: $(SHELLNAME) validtestperms
	$(TESTSCRIPT) -vt $(id)

describe: $(SHELLNAME)
	$(TESTSCRIPT) -d $(id)

grade: $(SHELLNAME) validtestperms
	$(TESTSCRIPT) --compute-score

#####################################
# Rules intended for instructor use #
#####################################

HANDOUT_FILES = examples/ tests/\
 argprinter.c fib.c \
 Makefile README.shell \
 shell_design.txt\
 mykill.c handle.c\
 utcsh.c util.c utcsh.r util.h
# shellspec.md   # Not included at the moment because it's so new.

handout: $(HANDOUT_FILES)
	tar czvf shell_project.tar.gz $^
