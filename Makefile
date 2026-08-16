#
# This is free and unencumbered software released into the public domain.
# See the UNLICENSE file or http://unlicense.org/ for details.
#

CC ?= gcc
CFLAGS ?= -std=c90 -pedantic -Wall -Wextra -O2 -Iinclude
SRCS = src/main.c src/util.c src/lex.c src/cpp.c src/type.c src/sym.c src/parse.c src/gen.c
OBJS = $(SRCS:.c=.o)
TARGET = cc90

STAGE2_OBJS = $(SRCS:.c=.stage2.o)
STAGE2_TARGET = cc90_stage2
STAGE3_OBJS = $(SRCS:.c=.stage3.o)
STAGE3_TARGET = cc90_stage3

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Stage 2: Built using Stage 1 cc90
%.stage2.o: %.c $(TARGET)
	./$(TARGET) -Iinclude -c -o $@ $<

$(STAGE2_TARGET): $(STAGE2_OBJS)
	$(CC) -o $@ $(STAGE2_OBJS)

# Stage 3: Built using Stage 2 cc90_stage2
%.stage3.o: %.c $(STAGE2_TARGET)
	./$(STAGE2_TARGET) -Iinclude -c -o $@ $<

$(STAGE3_TARGET): $(STAGE3_OBJS)
	$(CC) -o $@ $(STAGE3_OBJS)

# Self-hosting bootstrap verification
self: $(STAGE2_TARGET) $(STAGE3_TARGET)
	@echo "=== Verifying self-compilation and assembly reproducibility ==="
	@for f in $(SRCS); do \
		./$(STAGE2_TARGET) -Iinclude -S -o $${f%.c}.s2.s $$f || exit 1; \
		./$(STAGE3_TARGET) -Iinclude -S -o $${f%.c}.s3.s $$f || exit 1; \
		diff -u $${f%.c}.s2.s $${f%.c}.s3.s || { echo "Bootstrap diff mismatch in $$f"; exit 1; }; \
	done
	@echo "SUCCESS: Stage 2 and Stage 3 produced 100% identical assembly! Fixed-point reached."

bootstrap: self

test: $(TARGET)
	@bash tests/run_tests.sh

test-self: self
	@echo "=== Running test suite with Stage 1 (cc90) ==="
	@CC90=./$(TARGET) bash tests/run_tests.sh
	@echo "=== Running test suite with Stage 2 (cc90_stage2) ==="
	@CC90=./$(STAGE2_TARGET) bash tests/run_tests.sh
	@echo "=== Running test suite with Stage 3 (cc90_stage3) ==="
	@CC90=./$(STAGE3_TARGET) bash tests/run_tests.sh
	@echo "SUCCESS: All stages passed all test suites!"

clean:
	rm -f $(OBJS) $(STAGE2_OBJS) $(STAGE3_OBJS)
	rm -f $(TARGET) $(STAGE2_TARGET) $(STAGE3_TARGET)
	rm -f *.o *.s *.stage2.s *.stage3.s *.s2.s *.s3.s src/*.s src/*.s2.s src/*.s3.s src/*.tmp.s
	rm -f a.out tests/*.o tests/*.s tests/*.bin

.PHONY: all test test-self self bootstrap clean

