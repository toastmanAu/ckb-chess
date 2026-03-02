# Build chess contract for CKB-VM (RISC-V target)
# Requires: riscv64-unknown-elf-gcc or clang with RISC-V target

CC_RISCV = riscv64-unknown-elf-gcc
CC_HOST  = gcc

CFLAGS   = -O2 -Wall -Wextra -std=c99 -I src

SRCS_CORE = src/chess.c src/chess_moves.c

# Host build (for testing — not for deployment)
test: $(SRCS_CORE) src/chess.h tests/test_chess.c
	$(CC_HOST) $(CFLAGS) -DHOST_TEST $(SRCS_CORE) tests/test_chess.c -o chess_test
	./chess_test

# RISC-V build (for CKB-VM)
contract: $(SRCS_CORE) src/entry.c src/chess.h
	$(CC_RISCV) $(CFLAGS) -nostdlib -nostartfiles \
		$(SRCS_CORE) src/entry.c \
		-o chess-contract.elf
	riscv64-unknown-elf-objcopy -O binary chess-contract.elf chess-contract.bin

clean:
	rm -f chess_test chess-contract.elf chess-contract.bin

.PHONY: test contract clean
