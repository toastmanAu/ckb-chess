/*
 * entry.c — CKB script entry point for the chess contract
 *
 * This script runs on CKB-VM (RISC-V) when a game cell is consumed.
 * It handles three cases:
 *   1. open_game   — both players commit funds and pubkeys
 *   2. close_game  — cooperative: both sign outcome, funds released
 *   3. dispute     — one player submits full move log; contract validates
 *   4. timeout     — player claims timeout after N blocks without a move
 *
 * Stub implementation — full CKB syscall integration pending.
 */

/* CKB syscall stubs (replace with ckb-c-stdlib includes for real build) */
typedef unsigned long long uint64_t;
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;

int ckb_load_script(void *addr, uint64_t *len, size_t offset);
int ckb_load_cell_data(void *addr, uint64_t *len, size_t offset, size_t index, size_t source);
int ckb_load_header(void *addr, uint64_t *len, size_t offset, size_t index, size_t source);

#include "chess.h"

/* Game action type — determined by witness data */
typedef enum {
    ACTION_OPEN    = 0,
    ACTION_CLOSE   = 1,
    ACTION_DISPUTE = 2,
    ACTION_TIMEOUT = 3,
} GameAction;

int main() {
    /* TODO: Load script args (game pubkeys, stake amounts, time control) */
    /* TODO: Load witness (action type + move log + signatures) */
    /* TODO: Dispatch to handler based on action */

    /* For dispute resolution:
     *   1. Load the full move log from witness
     *   2. Initialise board with chess_init()
     *   3. For each move in log:
     *      a. Verify player signature over (prev_hash || move || seq)
     *      b. chess_apply_move() — returns error if illegal
     *   4. Determine winner from final board state
     *   5. Verify output cell goes to winner's lock hash
     */

    return 0; /* 0 = success in CKB scripts */
}
