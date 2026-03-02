/*
 * test_chess.c — Host tests for chess validator
 * Build: gcc -std=c99 -I src src/chess.c src/chess_moves.c tests/test_chess.c -o chess_test
 */

#include <stdio.h>
#include <assert.h>
#include "chess.h"

/* Forward declarations */
static void test_castling_kingside(void);
static void test_knight_jump(void);
static void test_pawn_capture_diagonal(void);
static void test_pawn_cant_capture_forward(void);
static void test_king_cant_move_into_check(void);
static void test_promotion(void);


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
    test_castling_kingside();
    test_knight_jump();
    test_pawn_capture_diagonal();
    test_pawn_cant_capture_forward();
    test_king_cant_move_into_check();
    test_promotion();
    printf("=== all 15 tests passed ===\n");
    return 0;
}

/* ── Additional move validation tests ──────────────────────────────────────── */

static void test_castling_kingside() {
    Board b; chess_init(&b);
    b.squares[0][5] = EMPTY; /* f1 */
    b.squares[0][6] = EMPTY; /* g1 */
    Move mv = {0, 4, 0, 6, 0};
    int r = chess_apply_move(&b, &mv);
    assert(r == CHESS_OK);
    assert(b.squares[0][6] == 'K');
    assert(b.squares[0][5] == 'R');
    printf("✓ kingside castling\n");
}

static void test_knight_jump() {
    Board b; chess_init(&b);
    Move mv = {0, 6, 2, 5, 0}; /* Ng1→f3 */
    int r = chess_apply_move(&b, &mv);
    assert(r == CHESS_OK);
    printf("✓ knight jump\n");
}

static void test_pawn_capture_diagonal() {
    Board b; chess_init(&b);
    b.squares[1][4] = EMPTY; b.squares[3][4] = 'P'; /* e4 */
    b.squares[6][3] = EMPTY; b.squares[4][3] = 'p'; /* d5 */
    b.white_to_move = 1;
    Move mv = {3, 4, 4, 3, 0};
    int r = chess_apply_move(&b, &mv);
    assert(r == CHESS_OK);
    assert(b.squares[4][3] == 'P');
    printf("✓ pawn diagonal capture\n");
}

static void test_pawn_cant_capture_forward() {
    Board b; chess_init(&b);
    b.squares[2][4] = 'p';
    b.white_to_move = 1;
    Move mv = {1, 4, 2, 4, 0};
    int r = chess_apply_move(&b, &mv);
    assert(r != CHESS_OK);
    printf("✓ blocked pawn rejected\n");
}

static void test_king_cant_move_into_check() {
    Board b; chess_init(&b);
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) b.squares[r][f]=EMPTY;
    b.squares[0][4] = 'K';
    b.squares[7][4] = 'r';
    b.white_to_move = 1;
    Move mv = {0, 4, 1, 4, 0};
    int r2 = chess_apply_move(&b, &mv);
    assert(r2 != CHESS_OK);
    printf("✓ king can't move into check\n");
}

static void test_promotion() {
    Board b; chess_init(&b);
    for (int r=0;r<8;r++) for (int f=0;f<8;f++) b.squares[r][f]=EMPTY;
    b.squares[6][4] = 'P';
    b.squares[0][4] = 'K';
    b.squares[7][0] = 'k';
    b.white_to_move = 1;
    Move mv = {6, 4, 7, 4, 'q'};
    int r = chess_apply_move(&b, &mv);
    assert(r == CHESS_OK);
    assert(b.squares[7][4] == 'Q');
    printf("✓ pawn promotion to queen\n");
}
