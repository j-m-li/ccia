#
# This is free and unencumbered software released into the public domain.
# See the UNLICENSE file or http://unlicense.org/ for details.
#

CC ?= gcc
CFLAGS ?= -std=c90 -pedantic -Wall -Wextra -O2 -Iinclude

# Default system include paths passed via -I options
SYS_INCLUDES_X86_64 = $(shell for d in \
	/usr/lib/gcc/x86_64-redhat-linux/*/include \
	/usr/lib/gcc/x86_64-linux-gnu/*/include \
	/usr/include/x86_64-linux-gnu \
	/usr/local/include \
	/usr/include; do [ -d "$$d" ] && echo "-I$$d"; done)

SYS_INCLUDES_I386 = $(shell for d in \
	/usr/lib/gcc/x86_64-redhat-linux/*/32/include \
	/usr/lib/gcc/x86_64-redhat-linux/*/include \
	/usr/lib/gcc/x86_64-linux-gnu/*/32/include \
	/usr/lib/gcc/x86_64-linux-gnu/*/include \
	/usr/lib/gcc/i686-linux-gnu/*/include \
	/usr/include/i386-linux-gnu \
	/usr/local/include \
	/usr/include; do [ -d "$$d" ] && echo "-I$$d"; done)

INCLUDES_X86_64 = -Iinclude $(SYS_INCLUDES_X86_64)
INCLUDES_I386 = -Iinclude $(SYS_INCLUDES_I386)

# x86_64 Sources & Target
SRCS_X86_64 = src/main.c src/util.c src/lex.c src/cpp.c src/type.c src/sym.c src/parse.c src/gen.c src/softfloat.c
OBJS_X86_64 = $(SRCS_X86_64:.c=.o)
TARGET_X86_64 = cc90

STAGE2_OBJS = $(SRCS_X86_64:.c=.stage2.o)
STAGE2_TARGET = cc90_stage2
STAGE3_OBJS = $(SRCS_X86_64:.c=.stage3.o)
STAGE3_TARGET = cc90_stage3

# i386 (x86 32-bit) Sources & Target
SRCS_I386 = src/main.c src/util.c src/lex.c src/cpp.c src/type.c src/sym.c src/parse.c src/gen_i386.c src/softfloat.c
OBJS_I386 = $(SRCS_I386:.c=.i386.o)
TARGET_I386 = cc90-i386

STAGE2_I386_OBJS = $(SRCS_I386:.c=.i386_s2.o)
STAGE2_I386_TARGET = cc90-i386_stage2
STAGE3_I386_OBJS = $(SRCS_I386:.c=.i386_s3.o)
STAGE3_I386_TARGET = cc90-i386_stage3

all: $(TARGET_X86_64) $(TARGET_I386) src/softfloat.o

# -----------------------------------------------------------------------------
# x86_64 Build & Bootstrap Targets
# -----------------------------------------------------------------------------

$(TARGET_X86_64): $(OBJS_X86_64)
	$(CC) $(CFLAGS) -o $@ $(OBJS_X86_64)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Stage 2: Built using Stage 1 cc90
%.stage2.o: %.c $(TARGET_X86_64)
	./$(TARGET_X86_64) $(INCLUDES_X86_64) -c -o $@ $<

$(STAGE2_TARGET): $(STAGE2_OBJS)
	$(CC) -o $@ $(STAGE2_OBJS)

# Stage 3: Built using Stage 2 cc90_stage2
%.stage3.o: %.c $(STAGE2_TARGET)
	./$(STAGE2_TARGET) $(INCLUDES_X86_64) -c -o $@ $<

$(STAGE3_TARGET): $(STAGE3_OBJS)
	$(CC) -o $@ $(STAGE3_OBJS)

# x86_64 Self-hosting bootstrap verification
self: $(STAGE2_TARGET) $(STAGE3_TARGET)
	@echo "=== Verifying x86_64 self-compilation and assembly reproducibility ==="
	@for f in $(SRCS_X86_64); do \
		./$(STAGE2_TARGET) $(INCLUDES_X86_64) -S -o $${f%.c}.s2.s $$f || exit 1; \
		./$(STAGE3_TARGET) $(INCLUDES_X86_64) -S -o $${f%.c}.s3.s $$f || exit 1; \
		diff -u $${f%.c}.s2.s $${f%.c}.s3.s || { echo "Bootstrap diff mismatch in $$f"; exit 1; }; \
	done
	@echo "SUCCESS: x86_64 Stage 2 and Stage 3 produced 100% identical assembly! Fixed-point reached."

bootstrap: self

# -----------------------------------------------------------------------------
# i386 (x86 32-bit) Build & Bootstrap Targets
# -----------------------------------------------------------------------------

$(TARGET_I386): $(OBJS_I386)
	$(CC) $(CFLAGS) -m32  -DTARGET_I386 -o $@ $(OBJS_I386)

%.i386.o: %.c
	$(CC) $(CFLAGS) -m32  -DTARGET_I386 -c -o $@ $<

# Stage 2 (i386): Built using Stage 1 cc90-i386
%.i386_s2.o: %.c $(TARGET_I386)
	./$(TARGET_I386) $(INCLUDES_I386) -DTARGET_I386 -c -o $@ $<

$(STAGE2_I386_TARGET): $(STAGE2_I386_OBJS)
	$(CC) -m32 -o $@ $(STAGE2_I386_OBJS)

# Stage 3 (i386): Built using Stage 2 cc90-i386_stage2
%.i386_s3.o: %.c $(STAGE2_I386_TARGET)
	./$(STAGE2_I386_TARGET) $(INCLUDES_I386) -DTARGET_I386 -c -o $@ $<

$(STAGE3_I386_TARGET): $(STAGE3_I386_OBJS)
	$(CC) -m32 -o $@ $(STAGE3_I386_OBJS)

# i386 Self-hosting bootstrap verification
self-i386: $(STAGE2_I386_TARGET) $(STAGE3_I386_TARGET)
	@echo "=== Verifying i386 self-compilation and assembly reproducibility ==="
	@for f in $(SRCS_I386); do \
		./$(STAGE2_I386_TARGET) $(INCLUDES_I386) -DTARGET_I386 -S -o $${f%.c}.i386_s2.s $$f || exit 1; \
		./$(STAGE3_I386_TARGET) $(INCLUDES_I386) -DTARGET_I386 -S -o $${f%.c}.i386_s3.s $$f || exit 1; \
		diff -u $${f%.c}.i386_s2.s $${f%.c}.i386_s3.s || { echo "i386 Bootstrap diff mismatch in $$f"; exit 1; }; \
	done
	@echo "SUCCESS: i386 Stage 2 and Stage 3 produced 100% identical assembly! Fixed-point reached."

bootstrap-i386: self-i386

# -----------------------------------------------------------------------------
# Testing Targets
# -----------------------------------------------------------------------------

test: $(TARGET_X86_64) $(TARGET_I386)
	@echo "=== Running test suite with cc90 (x86_64) ==="
	@CC90=./$(TARGET_X86_64) CC90_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@echo "=== Running test suite with cc90-i386 (x86 32-bit) ==="
	@CC90=./$(TARGET_I386) CC90_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@echo "SUCCESS: All tests passed on both x86_64 and i386!"

test-x86_64: $(TARGET_X86_64)
	@CC90=./$(TARGET_X86_64) CC90_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh

test-i386: $(TARGET_I386)
	@CC90=./$(TARGET_I386) CC90_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh

test-self: self self-i386
	@echo "=== Running test suite with x86_64 stages ==="
	@CC90=./$(TARGET_X86_64) CC90_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@CC90=./$(STAGE2_TARGET) CC90_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@CC90=./$(STAGE3_TARGET) CC90_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@echo "=== Running test suite with i386 stages ==="
	@CC90=./$(TARGET_I386) CC90_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@CC90=./$(STAGE2_I386_TARGET) CC90_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@CC90=./$(STAGE3_I386_TARGET) CC90_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@echo "SUCCESS: All stages passed all test suites on both architectures!"

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------

clean:
	rm -f $(OBJS_X86_64) $(STAGE2_OBJS) $(STAGE3_OBJS)
	rm -f $(OBJS_I386) $(STAGE2_I386_OBJS) $(STAGE3_I386_OBJS)
	rm -f $(TARGET_X86_64) $(STAGE2_TARGET) $(STAGE3_TARGET)
	rm -f $(TARGET_I386) $(STAGE2_I386_TARGET) $(STAGE3_I386_TARGET)
	rm -f *.o *.s *.stage2.s *.stage3.s *.s2.s *.s3.s *.i386*.s src/*.s src/*.s2.s src/*.s3.s src/*.i386*.s src/*.tmp.s
	rm -f a.out tests/*.o tests/*.s tests/*.bin

.PHONY: all self self-i386 bootstrap bootstrap-i386 test test-x86_64 test-i386 test-self clean



