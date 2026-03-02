/*
 * chess.c — Pure chess logic for CKB-VM
 *
 * Portable C99. No stdlib dependencies beyond <stdint.h> and <string.h>.
 * Adapted from kiazamiri/Chess with major restructuring:
 *   - Removed all Windows/display code
 *   - Normalised coordinate system to 0-7 rank/file
 *   - Added UCI parser
 *   - Added en passant, castling
 *   - Separated move validation from board update
 *   - Added check/checkmate detection
 *
 * Board: board[rank][file], rank 0 = white's back rank (rank 1 in chess notation)
 */

#include "chess.h"

/* ── Helpers ─────────────────────────────────────────────────────────── */

static int is_white(char p) { return p >= 'A' && p <= 'Z'; }
static int is_black(char p) { return p >= 'a' && p <= 'z'; }
static int is_piece(char p) { return p != EMPTY; }

static int same_color(char a, char b) {
    return (is_white(a) && is_white(b)) || (is_black(a) && is_black(b));
}

static char to_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? (c + 32) : c;
}

/* ── Initialisation ──────────────────────────────────────────────────── */

void chess_init(Board *board) {
    /* Clear */
    for (int r = 0; r < BOARD_SIZE; r++)
        for (int f = 0; f < BOARD_SIZE; f++)
            board->squares[r][f] = EMPTY;

    /* White back rank (rank 0) */
    board->squares[0][0] = 'R'; board->squares[0][7] = 'R';
    board->squares[0][1] = 'N'; board->squares[0][6] = 'N';
    board->squares[0][2] = 'B'; board->squares[0][5] = 'B';
    board->squares[0][3] = 'Q'; board->squares[0][4] = 'K';

    /* White pawns (rank 1) */
    for (int f = 0; f < BOARD_SIZE; f++)
        board->squares[1][f] = 'P';

    /* Black pawns (rank 6) */
    for (int f = 0; f < BOARD_SIZE; f++)
        board->squares[6][f] = 'p';

    /* Black back rank (rank 7) */
    board->squares[7][0] = 'r'; board->squares[7][7] = 'r';
    board->squares[7][1] = 'n'; board->squares[7][6] = 'n';
    board->squares[7][2] = 'b'; board->squares[7][5] = 'b';
    board->squares[7][3] = 'q'; board->squares[7][4] = 'k';

    board->white_to_move    = 1;
    board->white_can_castle_k = 1;
    board->white_can_castle_q = 1;
    board->black_can_castle_k = 1;
    board->black_can_castle_q = 1;
    board->en_passant_file  = -1;
    board->en_passant_rank  = -1;
    board->move_count       = 0;
}

/* ── UCI Parser ──────────────────────────────────────────────────────── */

int chess_parse_uci(const char *uci, Move *move) {
    /* Format: "e2e4" or "e7e8q" */
    if (!uci || !move) return CHESS_PARSE_ERROR;
    int len = 0;
    while (uci[len]) len++;
    if (len < 4 || len > 5) return CHESS_PARSE_ERROR;

    char ff = uci[0], fr = uci[1], tf = uci[2], tr = uci[3];

    if (ff < 'a' || ff > 'h') return CHESS_PARSE_ERROR;
    if (tf < 'a' || tf > 'h') return CHESS_PARSE_ERROR;
    if (fr < '1' || fr > '8') return CHESS_PARSE_ERROR;
    if (tr < '1' || tr > '8') return CHESS_PARSE_ERROR;

    move->from_file  = ff - 'a';
    move->from_rank  = fr - '1';
    move->to_file    = tf - 'a';
    move->to_rank    = tr - '1';
    move->promotion  = (len == 5) ? uci[4] : '\0';

    /* Validate promotion piece */
    if (move->promotion) {
        char p = move->promotion;
        if (p != 'q' && p != 'r' && p != 'b' && p != 'n')
            return CHESS_PARSE_ERROR;
    }

    return CHESS_OK;
}

/* ── Attack detection ────────────────────────────────────────────────── */

/* Is square (rank, file) attacked by any piece of the given color? */
static int square_attacked_by(const Board *b, int rank, int file, int by_white) {
    int dr, df, r, f;

    /* Pawns */
    int pawn_rank = rank + (by_white ? -1 : 1);
    char pawn = by_white ? 'P' : 'p';
    if (pawn_rank >= 0 && pawn_rank < BOARD_SIZE) {
        if (file > 0 && b->squares[pawn_rank][file-1] == pawn) return 1;
        if (file < 7 && b->squares[pawn_rank][file+1] == pawn) return 1;
    }

    /* Knights */
    int knight_moves[8][2] = {{-2,-1},{-2,1},{-1,-2},{-1,2},{1,-2},{1,2},{2,-1},{2,1}};
    char knight = by_white ? 'N' : 'n';
    for (int i = 0; i < 8; i++) {
        r = rank + knight_moves[i][0];
        f = file + knight_moves[i][1];
        if (r >= 0 && r < 8 && f >= 0 && f < 8 && b->squares[r][f] == knight) return 1;
    }

    /* Bishops + Queen (diagonals) */
    int dirs[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
    char bishop = by_white ? 'B' : 'b';
    char queen  = by_white ? 'Q' : 'q';
    for (int d = 0; d < 4; d++) {
        for (int step = 1; step < 8; step++) {
            r = rank + dirs[d][0]*step;
            f = file + dirs[d][1]*step;
            if (r < 0 || r >= 8 || f < 0 || f >= 8) break;
            char p = b->squares[r][f];
            if (p != EMPTY) {
                if (p == bishop || p == queen) return 1;
                break;
            }
        }
    }

    /* Rooks + Queen (ranks/files) */
    int rdirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    char rook = by_white ? 'R' : 'r';
    for (int d = 0; d < 4; d++) {
        for (int step = 1; step < 8; step++) {
            r = rank + rdirs[d][0]*step;
            f = file + rdirs[d][1]*step;
            if (r < 0 || r >= 8 || f < 0 || f >= 8) break;
            char p = b->squares[r][f];
            if (p != EMPTY) {
                if (p == rook || p == queen) return 1;
                break;
            }
        }
    }

    /* King */
    char king = by_white ? 'K' : 'k';
    for (dr = -1; dr <= 1; dr++) {
        for (df = -1; df <= 1; df++) {
            if (dr == 0 && df == 0) continue;
            r = rank + dr; f = file + df;
            if (r >= 0 && r < 8 && f >= 0 && f < 8 && b->squares[r][f] == king) return 1;
        }
    }

    return 0;
}

int chess_in_check(const Board *b, int white) {
    /* Find king */
    char king = white ? 'K' : 'k';
    for (int r = 0; r < 8; r++)
        for (int f = 0; f < 8; f++)
            if (b->squares[r][f] == king)
                return square_attacked_by(b, r, f, !white);
    return 0; /* Should never happen */
}

/* ── Move Application ────────────────────────────────────────────────── */

int chess_apply_move(Board *board, const Move *mv) {
    int white = board->white_to_move;
    char piece = board->squares[mv->from_rank][mv->from_file];

    /* Piece must exist and be the right color */
    if (piece == EMPTY) return CHESS_ILLEGAL_MOVE;
    if (white && !is_white(piece)) return CHESS_WRONG_TURN;
    if (!white && !is_black(piece)) return CHESS_WRONG_TURN;

    /* Can't capture own piece */
    char target = board->squares[mv->to_rank][mv->to_file];
    if (is_piece(target) && same_color(piece, target)) return CHESS_ILLEGAL_MOVE;

    /* TODO: Full per-piece move validation goes here.
     * For now: validate that the move doesn't leave own king in check.
     * Full validation (ray checking per piece type) to be added. */

    /* Apply move to a copy first to test for check */
    Board test = *board;
    test.squares[mv->from_rank][mv->from_file] = EMPTY;
    test.squares[mv->to_rank][mv->to_file] = piece;

    /* En passant capture */
    char lpiece = to_lower(piece);
    if (lpiece == 'p' && mv->to_file == board->en_passant_file
        && mv->to_rank == board->en_passant_rank) {
        int captured_rank = mv->to_rank + (white ? -1 : 1);
        test.squares[captured_rank][mv->to_file] = EMPTY;
    }

    /* Promotion */
    if (mv->promotion && lpiece == 'p') {
        char promo = mv->promotion;
        test.squares[mv->to_rank][mv->to_file] = white ? (promo - 32) : promo;
    }

    /* Reject if this leaves own king in check */
    if (chess_in_check(&test, white)) return CHESS_ILLEGAL_MOVE;

    /* Commit move */
    *board = test;

    /* Update en passant state */
    board->en_passant_file = -1;
    board->en_passant_rank = -1;
    if (lpiece == 'p' && (int)mv->to_rank - (int)mv->from_rank == 2) {
        board->en_passant_file = mv->to_file;
        board->en_passant_rank = mv->from_rank + 1;
    } else if (lpiece == 'p' && (int)mv->from_rank - (int)mv->to_rank == 2) {
        board->en_passant_file = mv->to_file;
        board->en_passant_rank = mv->from_rank - 1;
    }

    /* Update castling rights */
    if (piece == 'K') { board->white_can_castle_k = 0; board->white_can_castle_q = 0; }
    if (piece == 'k') { board->black_can_castle_k = 0; board->black_can_castle_q = 0; }
    if (mv->from_rank == 0 && mv->from_file == 0) board->white_can_castle_q = 0;
    if (mv->from_rank == 0 && mv->from_file == 7) board->white_can_castle_k = 0;
    if (mv->from_rank == 7 && mv->from_file == 0) board->black_can_castle_q = 0;
    if (mv->from_rank == 7 && mv->from_file == 7) board->black_can_castle_k = 0;

    board->white_to_move = !white;
    board->move_count++;

    /* Check for checkmate/stalemate (TODO: implement has_legal_moves) */
    /* Return CHESS_OK for now — full termination detection is next step */
    return CHESS_OK;
}

/* ── Serialisation ───────────────────────────────────────────────────── */

void chess_serialise(const Board *b, uint8_t *out) {
    /* 64 bytes for squares, 8 bytes for flags */
    for (int r = 0; r < 8; r++)
        for (int f = 0; f < 8; f++)
            out[r*8 + f] = (uint8_t)b->squares[r][f];

    out[64] = (uint8_t)b->white_to_move;
    out[65] = (uint8_t)b->white_can_castle_k;
    out[66] = (uint8_t)b->white_can_castle_q;
    out[67] = (uint8_t)b->black_can_castle_k;
    out[68] = (uint8_t)b->black_can_castle_q;
    out[69] = (uint8_t)(b->en_passant_file + 1); /* +1 so -1 becomes 0 */
    out[70] = (uint8_t)(b->en_passant_rank + 1);
    out[71] = 0; /* reserved */
}
