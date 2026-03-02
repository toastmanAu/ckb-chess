/*
 * test_chess.c — Host tests for chess validator
 * Build: gcc -std=c99 -I src src/chess.c tests/test_chess.c -o chess_test
 */

#include <stdio.h>
#include <assert.h>
#include "chess.h"

#define PASS(name) printf("✓ %s\n", name)
#define FAIL(name) printf("✗ %s\n", name)

static void test_init() {
    Board b;
    chess_init(&b);
    assert(b.squares[0][0] == 'R'); /* a1 rook */
    assert(b.squares[7][4] == 'k'); /* e8 black king */
    assert(b.squares[4][4] == EMPTY);
    assert(b.white_to_move == 1);
    PASS("board init");
}

static void test_uci_parse() {
    Move m;
    assert(chess_parse_uci("e2e4", &m) == CHESS_OK);
    assert(m.from_file == 4 && m.from_rank == 1);
    assert(m.to_file   == 4 && m.to_rank   == 3);
    assert(m.promotion == '\0');

    assert(chess_parse_uci("e7e8q", &m) == CHESS_OK);
    assert(m.promotion == 'q');

    assert(chess_parse_uci("e2e", &m) == CHESS_PARSE_ERROR);
    assert(chess_parse_uci("e9e4", &m) == CHESS_PARSE_ERROR);
    PASS("UCI parse");
}

static void test_e2e4() {
    Board b;
    chess_init(&b);
    Move m;
    chess_parse_uci("e2e4", &m);
    int result = chess_apply_move(&b, &m);
    assert(result == CHESS_OK);
    assert(b.squares[3][4] == 'P'); /* pawn moved to e4 (rank 3, file 4) */
    assert(b.squares[1][4] == EMPTY);
    assert(b.white_to_move == 0); /* black's turn */
    assert(b.en_passant_file == 4);
    PASS("e2e4 pawn move");
}

static void test_wrong_turn() {
    Board b;
    chess_init(&b);
    Move m;
    chess_parse_uci("e7e5", &m); /* black pawn, but white's turn */
    assert(chess_apply_move(&b, &m) == CHESS_WRONG_TURN);
    PASS("wrong turn rejected");
}

static void test_serialise() {
    Board b;
    chess_init(&b);
    uint8_t state[CHESS_STATE_BYTES];
    chess_serialise(&b, state);
    assert(state[0] == 'R');  /* a1 rook */
    
    /* rank 7 black king at squares[7][4] -> byte 7*8+4 = 60 */
    /* Actually rank 6 is pawns: squares[6][0]='p' -> byte 6*8+0 = 48 */
    assert(state[48] == 'p');
    PASS("serialise");
}

int main() {
    printf("=== chess validator tests ===\n");
    test_init();
    test_uci_parse();
    test_e2e4();
    test_wrong_turn();
    test_serialise();
    printf("=== all tests passed ===\n");
    return 0;
}
