#
# This is free and unencumbered software released into the public domain.
# See the UNLICENSE file or http://unlicense.org/ for details.
#

CC ?= clang
CFLAGS ?= -std=c90 -pedantic -Wall -Wextra -O2 -Iinclude

# Default system include paths passed via -I options
SYS_INCLUDES_X86_64 = $(shell for d in \
	/usr/lib64/clang/*/include \
	/usr/lib/clang/*/include \
	/usr/lib/gcc/x86_64-redhat-linux/*/include \
	/usr/lib/gcc/x86_64-linux-gnu/*/include \
	/usr/include/x86_64-linux-gnu \
	/usr/local/include \
	/usr/include; do [ -d "$$d" ] && echo "-I$$d"; done)

SYS_INCLUDES_I386 = $(shell for d in \
	/usr/lib64/clang/*/include \
	/usr/lib/clang/*/include \
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
TARGET_X86_64 = ccia

STAGE2_OBJS = $(SRCS_X86_64:.c=.stage2.o)
STAGE2_TARGET = ccia_stage2
STAGE3_OBJS = $(SRCS_X86_64:.c=.stage3.o)
STAGE3_TARGET = ccia_stage3

# i386 (x86 32-bit) Sources & Target
SRCS_I386 = src/main.c src/util.c src/lex.c src/cpp.c src/type.c src/sym.c src/parse.c src/gen_i386.c src/softfloat.c
OBJS_I386 = $(SRCS_I386:.c=.i386.o)
TARGET_I386 = ccia-i386

STAGE2_I386_OBJS = $(SRCS_I386:.c=.i386_s2.o)
STAGE2_I386_TARGET = ccia-i386_stage2
STAGE3_I386_OBJS = $(SRCS_I386:.c=.i386_s3.o)
STAGE3_I386_TARGET = ccia-i386_stage3

# RISC-V 32-bit (RV32I) Sources & Target
CLANG ?= clang
QEMU_RV32 ?= qemu-riscv32-static
SRCS_RV32I = src/main.c src/util.c src/lex.c src/cpp.c src/type.c src/sym.c src/parse.c src/gen_riscv32.c src/softfloat.c
OBJS_RV32I = $(SRCS_RV32I:.c=.rv32i_host.o)
TARGET_RV32I = ccia-rv32i
STAGE2_RV32I_OBJS = $(SRCS_RV32I:.c=.rv32i_s2.o)
STAGE2_RV32I_TARGET = ccia-rv32i_stage2
STAGE3_RV32I_OBJS = $(SRCS_RV32I:.c=.rv32i_s3.o)
STAGE3_RV32I_TARGET = ccia-rv32i_stage3

RUNTIME_RV32I_OBJ = src/runtime_rv32.rv32i.o
SOFTFLOAT_RV32I_OBJ = src/softfloat.rv32i.o

all: $(TARGET_X86_64) $(TARGET_I386) $(TARGET_RV32I) src/softfloat.o

# -----------------------------------------------------------------------------
# x86_64 Build & Bootstrap Targets
# -----------------------------------------------------------------------------

$(TARGET_X86_64): $(OBJS_X86_64)
	$(CC) $(CFLAGS) -o $@ $(OBJS_X86_64)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Stage 2: Built using Stage 1 ccia
%.stage2.o: %.c $(TARGET_X86_64)
	./$(TARGET_X86_64) $(INCLUDES_X86_64) -c -o $@ $<

$(STAGE2_TARGET): $(STAGE2_OBJS)
	$(CC) -o $@ $(STAGE2_OBJS)

# Stage 3: Built using Stage 2 ccia_stage2
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

# Stage 2 (i386): Built using Stage 1 ccia-i386
%.i386_s2.o: %.c $(TARGET_I386)
	./$(TARGET_I386) $(INCLUDES_I386) -DTARGET_I386 -c -o $@ $<

$(STAGE2_I386_TARGET): $(STAGE2_I386_OBJS)
	$(CC) -m32 -o $@ $(STAGE2_I386_OBJS)

# Stage 3 (i386): Built using Stage 2 ccia-i386_stage2
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
# RISC-V 32-bit (RV32I) Build & Bootstrap Targets (using Clang)
# -----------------------------------------------------------------------------

$(TARGET_RV32I): $(OBJS_RV32I) $(RUNTIME_RV32I_OBJ) $(SOFTFLOAT_RV32I_OBJ)
	$(CC) $(CFLAGS) -DTARGET_RISCV32 -o $@ $(OBJS_RV32I)

%.rv32i_host.o: %.c
	$(CC) $(CFLAGS) -DTARGET_RISCV32 -c -o $@ $<

$(RUNTIME_RV32I_OBJ): src/runtime_rv32.c
	$(CLANG) --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -DTARGET_RISCV32 -Iinclude -O2 -c -o $@ $<

$(SOFTFLOAT_RV32I_OBJ): src/softfloat.c
	$(CLANG) --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -DTARGET_RISCV32 -Iinclude -O2 -c -o $@ $<

INCLUDES_RV32I = -Iinclude/riscv32 -Iinclude

# Stage 2 (RV32I): Built using Stage 1 ccia-rv32i
%.rv32i_s2.s: %.c $(TARGET_RV32I)
	./$(TARGET_RV32I) $(INCLUDES_RV32I) -DTARGET_RISCV32 -S -o $@ $<

%.rv32i_s2.o: %.rv32i_s2.s
	$(CLANG) --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -c -o $@ $<

$(STAGE2_RV32I_TARGET): $(STAGE2_RV32I_OBJS) $(RUNTIME_RV32I_OBJ)
	$(CLANG) --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -fuse-ld=lld -nostdlib -static -o $@ $(STAGE2_RV32I_OBJS) $(RUNTIME_RV32I_OBJ)

# Stage 3 (RV32I): Built using Stage 2 ccia-rv32i_stage2 under QEMU
%.rv32i_s3.s: %.c $(STAGE2_RV32I_TARGET)
	$(QEMU_RV32) ./$(STAGE2_RV32I_TARGET) $(INCLUDES_RV32I) -DTARGET_RISCV32 -S -o $@ $<

%.rv32i_s3.o: %.rv32i_s3.s
	$(CLANG) --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -c -o $@ $<

$(STAGE3_RV32I_TARGET): $(STAGE3_RV32I_OBJS) $(RUNTIME_RV32I_OBJ)
	$(CLANG) --target=riscv32-unknown-linux-gnu -march=rv32i -mabi=ilp32 -fuse-ld=lld -nostdlib -static -o $@ $(STAGE3_RV32I_OBJS) $(RUNTIME_RV32I_OBJ)

# RV32I Self-hosting bootstrap verification
self-rv32i: $(STAGE2_RV32I_TARGET) $(STAGE3_RV32I_TARGET)
	@echo "=== Verifying RV32I self-compilation and assembly reproducibility ==="
	@for f in $(SRCS_RV32I); do \
		$(QEMU_RV32) ./$(STAGE2_RV32I_TARGET) $(INCLUDES_RV32I) -DTARGET_RISCV32 -S -o $${f%.c}.rv32i_s2.s $$f || exit 1; \
		$(QEMU_RV32) ./$(STAGE3_RV32I_TARGET) $(INCLUDES_RV32I) -DTARGET_RISCV32 -S -o $${f%.c}.rv32i_s3.s $$f || exit 1; \
		diff -u $${f%.c}.rv32i_s2.s $${f%.c}.rv32i_s3.s || { echo "RV32I Bootstrap diff mismatch in $$f"; exit 1; }; \
	done
	@echo "SUCCESS: RV32I Stage 2 and Stage 3 produced 100% identical assembly! Fixed-point reached."

bootstrap-rv32i: self-rv32i

# -----------------------------------------------------------------------------
# Testing Targets
# -----------------------------------------------------------------------------

test: $(TARGET_X86_64) $(TARGET_I386) $(TARGET_RV32I)
	@echo "=== Running test suite with ccia (x86_64) ==="
	@CCIA=./$(TARGET_X86_64) CCIA_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@echo "=== Running test suite with ccia-i386 (x86 32-bit) ==="
	@CCIA=./$(TARGET_I386) CCIA_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@echo "=== Running test suite with ccia-rv32i (RISC-V 32-bit RV32I) ==="
	@CCIA=./$(TARGET_RV32I) CCIA_RUNNER="$(QEMU_RV32)" CCIA_INCLUDES="$(INCLUDES_RV32I)" bash tests/run_tests.sh
	@echo "SUCCESS: All tests passed on x86_64, i386, and RISC-V 32-bit RV32I!"

test-x86_64: $(TARGET_X86_64)
	@CCIA=./$(TARGET_X86_64) CCIA_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh

test-i386: $(TARGET_I386)
	@CCIA=./$(TARGET_I386) CCIA_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh

test-rv32i: $(TARGET_RV32I)
	@CCIA=./$(TARGET_RV32I) CCIA_RUNNER="$(QEMU_RV32)" CCIA_INCLUDES="$(INCLUDES_RV32I)" bash tests/run_tests.sh

test-self: self self-i386 self-rv32i
	@echo "=== Running test suite with x86_64 stages ==="
	@CCIA=./$(TARGET_X86_64) CCIA_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@CCIA=./$(STAGE2_TARGET) CCIA_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@CCIA=./$(STAGE3_TARGET) CCIA_INCLUDES="$(INCLUDES_X86_64)" bash tests/run_tests.sh
	@echo "=== Running test suite with i386 stages ==="
	@CCIA=./$(TARGET_I386) CCIA_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@CCIA=./$(STAGE2_I386_TARGET) CCIA_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@CCIA=./$(STAGE3_I386_TARGET) CCIA_INCLUDES="$(INCLUDES_I386)" bash tests/run_tests.sh
	@echo "=== Running test suite with RV32I stages ==="
	@CCIA=./$(TARGET_RV32I) CCIA_RUNNER="$(QEMU_RV32)" CCIA_INCLUDES="$(INCLUDES_RV32I)" bash tests/run_tests.sh
	@CCIA=./$(STAGE2_RV32I_TARGET) CCIA_COMPILER_RUNNER="$(QEMU_RV32)" CCIA_RUNNER="$(QEMU_RV32)" CCIA_INCLUDES="$(INCLUDES_RV32I)" bash tests/run_tests.sh
	@CCIA=./$(STAGE3_RV32I_TARGET) CCIA_COMPILER_RUNNER="$(QEMU_RV32)" CCIA_RUNNER="$(QEMU_RV32)" CCIA_INCLUDES="$(INCLUDES_RV32I)" bash tests/run_tests.sh
	@echo "SUCCESS: All stages passed all test suites on x86_64, i386, and RV32I!"

# -----------------------------------------------------------------------------
# Clean
# -----------------------------------------------------------------------------

clean:
	rm -f $(OBJS_X86_64) $(STAGE2_OBJS) $(STAGE3_OBJS)
	rm -f $(OBJS_I386) $(STAGE2_I386_OBJS) $(STAGE3_I386_OBJS)
	rm -f $(OBJS_RV32I) $(RUNTIME_RV32I_OBJ) $(SOFTFLOAT_RV32I_OBJ) $(STAGE2_RV32I_OBJS) $(STAGE3_RV32I_OBJS)
	rm -f $(TARGET_X86_64) $(STAGE2_TARGET) $(STAGE3_TARGET)
	rm -f $(TARGET_I386) $(STAGE2_I386_TARGET) $(STAGE3_I386_TARGET)
	rm -f $(TARGET_RV32I) $(STAGE2_RV32I_TARGET) $(STAGE3_RV32I_TARGET)
	rm -f *.o *.s *.stage2.s *.stage3.s *.s2.s *.s3.s *.i386*.s *.rv32i*.s src/*.s src/*.s2.s src/*.s3.s src/*.i386*.s src/*.rv32i*.s src/*.tmp.s
	rm -f a.out tests/*.o tests/*.s tests/*.bin tests/stb.png tests/stb.jpg

triple-test: test-self
trpile-test: test-self

.PHONY: all self self-i386 self-rv32i bootstrap bootstrap-i386 bootstrap-rv32i test test-x86_64 test-i386 test-rv32i test-self triple-test trpile-test clean



