# Build chess contract for CKB-VM (RISC-V target)
# Requires: riscv64-unknown-elf-gcc or clang with RISC-V target

CC_RISCV = riscv64-unknown-elf-gcc
CC_HOST  = gcc

CFLAGS = -O2 -Wall -Wextra -std=c99 -I src

# Host build (for testing — not for deployment)
test: src/chess.c src/chess.h
	$(CC_HOST) $(CFLAGS) -DHOST_TEST src/chess.c tests/test_chess.c -o chess_test
	./chess_test

# RISC-V build (for CKB-VM)
contract: src/chess.c src/entry.c src/chess.h
	$(CC_RISCV) $(CFLAGS) -nostdlib -nostartfiles \
		src/chess.c src/entry.c \
		-o chess-contract.elf
	riscv64-unknown-elf-objcopy -O binary chess-contract.elf chess-contract.bin

clean:
	rm -f chess_test chess-contract.elf chess-contract.bin

.PHONY: test contract clean
