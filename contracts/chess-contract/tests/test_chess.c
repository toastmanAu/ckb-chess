/*
 * test_chess.c — Host tests for chess validator
 * Build: gcc -std=c99 -I src src/chess.c src/chess_moves.c tests/test_chess.c -o chess_test
 */

#include <stdio.h>
#include <assert.h>
#include "chess.h"

#define PASS(name) printf("✓ %s\n", name)

static void test_init() {
    Board b;
    chess_init(&b);
    assert(b.squares[0][0] == 'R');
    assert(b.squares[7][4] == 'k');
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
    PASS("UCI parse");
}

static void test_e2e4() {
    Board b;
    chess_init(&b);
    Move m;
    chess_parse_uci("e2e4", &m);
    assert(chess_apply_move(&b, &m) == CHESS_OK);
    assert(b.squares[3][4] == 'P');
    assert(b.squares[1][4] == EMPTY);
    assert(b.white_to_move == 0);
    assert(b.en_passant_file == 4);
    PASS("e2e4 pawn move");
}

static void test_wrong_turn() {
    Board b;
    chess_init(&b);
    Move m;
    chess_parse_uci("e7e5", &m);
    assert(chess_apply_move(&b, &m) == CHESS_WRONG_TURN);
    PASS("wrong turn rejected");
}

static void test_serialise() {
    Board b;
    chess_init(&b);
    uint8_t state[CHESS_STATE_BYTES];
    chess_serialise(&b, state);
    assert(state[0]  == 'R');
    assert(state[4]  == 'K');
    assert(state[60] == 'k');
    assert(state[48] == 'p');
    assert(state[64] == 1);
    PASS("serialise");
}

static void test_piece_validation() {
    Board b;
    chess_init(&b);
    Move m;
    /* Knight L-shape — valid */
    chess_parse_uci("g1f3", &m);
    assert(chess_validate_piece_move(&b, &m) == 1);
    /* Knight straight — invalid */
    chess_parse_uci("g1g3", &m);
    assert(chess_validate_piece_move(&b, &m) == 0);
    /* Bishop long diagonal blocked by own pawn (c1->e3 crosses d2 which has a pawn) */
    chess_parse_uci("c1e3", &m);
    assert(chess_validate_piece_move(&b, &m) == 0);
    PASS("piece validation");
}

static void test_has_legal_moves() {
    Board b;
    chess_init(&b);
    assert(chess_has_legal_moves(&b) == 1);
    PASS("has_legal_moves at start");
}

static void test_illegal_move_blocked() {
    Board b;
    chess_init(&b);
    Move m;
    /* Rook can't move — blocked by own pawn */
    chess_parse_uci("a1a3", &m);
    assert(chess_apply_move(&b, &m) == CHESS_ILLEGAL_MOVE);
    PASS("blocked rook rejected");
}

static void test_scholar_mate() {
    /* Scholar's mate — 4-move checkmate */
    Board b;
    chess_init(&b);
    Move m;
    chess_parse_uci("e2e4", &m); assert(chess_apply_move(&b, &m) == CHESS_OK);
    chess_parse_uci("e7e5", &m); assert(chess_apply_move(&b, &m) == CHESS_OK);
    chess_parse_uci("f1c4", &m); assert(chess_apply_move(&b, &m) == CHESS_OK);
    chess_parse_uci("b8c6", &m); assert(chess_apply_move(&b, &m) == CHESS_OK);
    chess_parse_uci("d1h5", &m); assert(chess_apply_move(&b, &m) == CHESS_OK);
    chess_parse_uci("a7a6", &m); assert(chess_apply_move(&b, &m) == CHESS_OK);
    chess_parse_uci("h5f7", &m);
    assert(chess_apply_move(&b, &m) == CHESS_CHECKMATE);
    PASS("scholar's mate — checkmate detected");
}

int main() {
    printf("=== chess validator tests ===\n");
    test_init();
    test_uci_parse();
    test_e2e4();
    test_wrong_turn();
    test_serialise();
    test_piece_validation();
    test_has_legal_moves();
    test_illegal_move_blocked();
    test_scholar_mate();
    printf("=== all %d tests passed ===\n", 9);
    return 0;
}
