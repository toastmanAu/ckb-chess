#ifndef CHESS_H
#define CHESS_H

#include <stdint.h>
#include <string.h>

/* ── Board representation ─────────────────────────────────────────────────
 * 8x8 board. Pieces encoded as chars (standard FEN convention):
 *   Uppercase = White: P N B R Q K
 *   Lowercase = Black: p n b r q k
 *   Empty      = '.'
 *
 * board[rank][file], rank 0 = rank 1 (white's back rank), rank 7 = rank 8
 * file  0 = a-file, file 7 = h-file
 */

#define EMPTY '.'
#define BOARD_SIZE 8

typedef struct {
    char squares[BOARD_SIZE][BOARD_SIZE];
    int  white_to_move;     /* 1 = white's turn, 0 = black's turn */
    int  white_can_castle_k; /* kingside */
    int  white_can_castle_q; /* queenside */
    int  black_can_castle_k;
    int  black_can_castle_q;
    int  en_passant_file;   /* -1 if none, 0-7 if en passant possible */
    int  en_passant_rank;
    uint32_t move_count;
} Board;

/* ── Move representation ─────────────────────────────────────────────────
 * from_rank, from_file, to_rank, to_file: 0-7
 * promotion: 0 = none, 'q'/'r'/'b'/'n' = promotion piece
 */
typedef struct {
    uint8_t from_rank;
    uint8_t from_file;
    uint8_t to_rank;
    uint8_t to_file;
    char    promotion; /* '\0' or 'q'/'r'/'b'/'n' */
} Move;

/* ── Result codes ─────────────────────────────────────────────────────── */
#define CHESS_OK            0
#define CHESS_ILLEGAL_MOVE  1
#define CHESS_CHECKMATE     2
#define CHESS_STALEMATE     3
#define CHESS_WRONG_TURN    4
#define CHESS_PARSE_ERROR   5

/* ── API ─────────────────────────────────────────────────────────────── */

/* Initialise board to standard starting position */
void chess_init(Board *board);

/* Parse UCI move string ("e2e4", "e7e8q") into Move struct.
 * Returns CHESS_OK or CHESS_PARSE_ERROR */
int chess_parse_uci(const char *uci, Move *move);

/* Validate and apply a move. Returns CHESS_OK on success.
 * On CHESS_OK, board is updated in place.
 * On CHESS_CHECKMATE/STALEMATE after the move, returns that code
 * (move was still valid, game is over). */
int chess_apply_move(Board *board, const Move *move);

/* Serialise board state to bytes for hashing.
 * out must be at least CHESS_STATE_BYTES long. */
#define CHESS_STATE_BYTES 72  /* 64 squares + 8 flags/metadata bytes */
void chess_serialise(const Board *board, uint8_t *out);

/* Is the given side's king currently in check? */
int chess_in_check(const Board *board, int white);

#endif /* CHESS_H */
