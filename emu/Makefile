CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -O2
LDFLAGS = -lm

SRCS = main.c halmat_engine.c halmat_loader.c halmat_float.c halmat_disasm.c \
       halmat_class0.c halmat_class1.c halmat_class2.c halmat_class34.c \
       halmat_class5.c halmat_class6.c halmat_class7.c halmat_class8.c \
       halmat_io.c halmat_debug.c

HDRS = halmat.h halmat_types.h halmat_io.h halmat_debug.h

OBJS = $(SRCS:.c=.o)

yaHALMAT: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) yaHALMAT yaHALMAT.exe

# Null I/O variant (for Orbiter integration)
yaHALMAT-null: $(filter-out halmat_io.o,$(OBJS)) halmat_io_null.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

halmat_io_null.o: halmat_io_null.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Test targets
test-disasm: yaHALMAT
	./yaHALMAT --disasm --litfile ../data/out_simple_do/litfile.bin ../data/out_simple_do/halmat.bin

test-simple: yaHALMAT
	./yaHALMAT ../data/out_simple_do/halmat.bin

test-ifelse: yaHALMAT
	./yaHALMAT ../data/out_ifelse/halmat.bin

test-while: yaHALMAT
	./yaHALMAT ../data/out_while/halmat.bin

test-all: yaHALMAT
	@echo "=== test_simple_do ===" && ./yaHALMAT ../data/out_simple_do/halmat.bin
	@echo "=== test_ifelse ===" && ./yaHALMAT ../data/out_ifelse/halmat.bin
	@echo "=== test_while ===" && ./yaHALMAT ../data/out_while/halmat.bin
	@echo "=== test_discrete_for ===" && ./yaHALMAT ../data/out_discrete_for/halmat.bin
	@echo "=== test_case ===" && ./yaHALMAT ../data/out_case/halmat.bin
	@echo "=== test_nested ===" && ./yaHALMAT ../data/out_nested/halmat.bin
	@echo "=== test_proc ===" && ./yaHALMAT ../data/out_proc/halmat.bin
	@echo "=== test_array ===" && ./yaHALMAT ../data/out_array/halmat.bin
	@echo "=== test_matrix ===" && ./yaHALMAT ../data/out_matrix/halmat.bin

.PHONY: clean test-disasm test-simple test-ifelse test-while test-all
