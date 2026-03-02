/*
 * chess_moves.c — Full per-piece move validation + legal move generation
 *
 * Replaces the TODO stubs in chess.c
 * Adds: has_legal_moves(), full piece ray validation, castling execution
 */

#include "chess.h"

/* ── Per-piece move validation ───────────────────────────────────────────────
 * Returns 1 if the piece at (from_rank, from_file) can legally move to
 * (to_rank, to_file) based purely on movement rules (ignoring check).
 * Check detection is handled by the caller (chess_apply_move).
 */

static int validate_pawn(const Board *b, const Move *mv) {
    int fr = mv->from_rank, ff = mv->from_file;
    int tr = mv->to_rank,   tf = mv->to_file;
    int white = b->white_to_move;
    int dir   = white ? 1 : -1;   /* white moves up (rank+), black moves down */
    int start = white ? 1 : 6;    /* starting rank */
    char target = b->squares[tr][tf];

    /* Forward one square — must be empty */
    if (tf == ff && tr == fr + dir && target == EMPTY)
        return 1;

    /* Forward two squares from start rank — both squares must be empty */
    if (tf == ff && fr == start && tr == fr + 2*dir
        && target == EMPTY && b->squares[fr + dir][ff] == EMPTY)
        return 1;

    /* Diagonal capture */
    if (tr == fr + dir && (tf == ff + 1 || tf == ff - 1)) {
        /* Normal capture */
        if (target != EMPTY && !( (white && target>='A'&&target<='Z') ||
                                  (!white && target>='a'&&target<='z') ))
            return 1;
        /* En passant */
        if (tf == b->en_passant_file && tr == b->en_passant_rank)
            return 1;
    }

    return 0;
}

static int validate_knight(const Board *b, const Move *mv) {
    int dr = mv->to_rank - mv->from_rank;
    int df = mv->to_file - mv->from_file;
    (void)b;
    return (dr*dr + df*df) == 5; /* 2²+1² = 5 — the L-shape */
}

static int validate_bishop(const Board *b, const Move *mv) {
    int fr = mv->from_rank, ff = mv->from_file;
    int tr = mv->to_rank,   tf = mv->to_file;
    int dr = tr - fr, df = tf - ff;

    if (dr == 0 || df == 0 || (dr < 0 ? -dr : dr) != (df < 0 ? -df : df))
        return 0; /* not diagonal */

    int sr = (dr > 0) ? 1 : -1;
    int sf = (df > 0) ? 1 : -1;
    int r = fr + sr, f = ff + sf;
    while (r != tr || f != tf) {
        if (b->squares[r][f] != EMPTY) return 0; /* blocked */
        r += sr; f += sf;
    }
    return 1;
}

static int validate_rook(const Board *b, const Move *mv) {
    int fr = mv->from_rank, ff = mv->from_file;
    int tr = mv->to_rank,   tf = mv->to_file;

    if (fr != tr && ff != tf) return 0; /* not rank or file */

    int sr = (tr > fr) ? 1 : (tr < fr) ? -1 : 0;
    int sf = (tf > ff) ? 1 : (tf < ff) ? -1 : 0;
    int r = fr + sr, f = ff + sf;
    while (r != tr || f != tf) {
        if (b->squares[r][f] != EMPTY) return 0;
        r += sr; f += sf;
    }
    return 1;
}

static int validate_queen(const Board *b, const Move *mv) {
    return validate_bishop(b, mv) || validate_rook(b, mv);
}

static int validate_king(const Board *b, const Move *mv) {
    int dr = mv->to_rank - mv->from_rank;
    int df = mv->to_file - mv->from_file;
    int adr = dr < 0 ? -dr : dr;
    int adf = df < 0 ? -df : df;

    /* Normal one-square king move */
    if (adr <= 1 && adf <= 1 && (adr + adf) > 0) return 1;

    /* Castling — two squares horizontally */
    if (adr == 0 && adf == 2) {
        int white = b->white_to_move;
        int back  = white ? 0 : 7;
        if (mv->from_rank != back || mv->from_file != 4) return 0;

        /* Kingside */
        if (mv->to_file == 6) {
            if (!(white ? b->white_can_castle_k : b->black_can_castle_k)) return 0;
            if (b->squares[back][5] != EMPTY || b->squares[back][6] != EMPTY) return 0;
            /* Must not castle through check */
            if (chess_in_check(b, white)) return 0;
            Board tmp = *b;
            tmp.squares[back][4] = EMPTY;
            tmp.squares[back][5] = white ? 'K' : 'k';
            if (chess_in_check(&tmp, white)) return 0;
            return 1;
        }
        /* Queenside */
        if (mv->to_file == 2) {
            if (!(white ? b->white_can_castle_q : b->black_can_castle_q)) return 0;
            if (b->squares[back][1] != EMPTY || b->squares[back][2] != EMPTY
                || b->squares[back][3] != EMPTY) return 0;
            if (chess_in_check(b, white)) return 0;
            Board tmp = *b;
            tmp.squares[back][4] = EMPTY;
            tmp.squares[back][3] = white ? 'K' : 'k';
            if (chess_in_check(&tmp, white)) return 0;
            return 1;
        }
    }
    return 0;
}

/* Public: validate movement for a specific piece type */
int chess_validate_piece_move(const Board *b, const Move *mv) {
    char piece = b->squares[mv->from_rank][mv->from_file];
    char lp = piece >= 'a' ? piece : piece + 32; /* to lower */
    switch (lp) {
        case 'p': return validate_pawn(b, mv);
        case 'n': return validate_knight(b, mv);
        case 'b': return validate_bishop(b, mv);
        case 'r': return validate_rook(b, mv);
        case 'q': return validate_queen(b, mv);
        case 'k': return validate_king(b, mv);
        default:  return 0;
    }
}

/* ── Castling rook move ───────────────────────────────────────────────────── */
void chess_apply_castling(Board *b, const Move *mv) {
    int back  = b->white_to_move ? 0 : 7;
    char rook = b->white_to_move ? 'R' : 'r';
    if (mv->to_file == 6) { /* kingside */
        b->squares[back][5] = rook;
        b->squares[back][7] = EMPTY;
    } else { /* queenside */
        b->squares[back][3] = rook;
        b->squares[back][0] = EMPTY;
    }
}

/* ── Legal move check ────────────────────────────────────────────────────── */

/* Try one candidate move — returns 1 if legal (piece moves correctly + no self-check) */
static int try_move(const Board *b, int fr, int ff, int tr, int tf, char promo) {
    Move mv = { (uint8_t)fr, (uint8_t)ff, (uint8_t)tr, (uint8_t)tf, promo };
    char piece  = b->squares[fr][ff];
    if (piece == EMPTY) return 0;

    int white = b->white_to_move;
    if (white  && !(piece >= 'A' && piece <= 'Z')) return 0;
    if (!white && !(piece >= 'a' && piece <= 'z')) return 0;

    char target = b->squares[tr][tf];
    if (target != EMPTY) {
        int target_white = target >= 'A' && target <= 'Z';
        if (white == target_white) return 0; /* can't capture own piece */
    }

    if (!chess_validate_piece_move(b, &mv)) return 0;

    /* Test for self-check */
    Board test = *b;
    test.squares[fr][ff] = EMPTY;
    test.squares[tr][tf] = piece;
    /* En passant */
    char lp = piece >= 'a' ? piece : piece + 32;
    if (lp == 'p' && tf == b->en_passant_file && tr == b->en_passant_rank) {
        int cap_rank = tr + (white ? -1 : 1);
        test.squares[cap_rank][tf] = EMPTY;
    }
    if (chess_in_check(&test, white)) return 0;
    return 1;
}

/* Returns 1 if the side to move has at least one legal move */
int chess_has_legal_moves(const Board *b) {
    for (int fr = 0; fr < 8; fr++) {
        for (int ff = 0; ff < 8; ff++) {
            char piece = b->squares[fr][ff];
            if (piece == EMPTY) continue;
            int is_white = piece >= 'A' && piece <= 'Z';
            if (is_white != b->white_to_move) continue;

            for (int tr = 0; tr < 8; tr++) {
                for (int tf = 0; tf < 8; tf++) {
                    if (try_move(b, fr, ff, tr, tf, '\0')) return 1;
                }
            }
        }
    }
    return 0;
}
