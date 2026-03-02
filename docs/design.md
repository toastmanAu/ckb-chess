# Protocol Design — CKB Chess

*Drafted: 2026-03-02. Origin: conversation with Matt (Nervos)*

## Guiding Principles

1. **CKB-VM validates, not ZK** — chess move validation is cheap and transparent. No hiding needed.
2. **Moves are signed state transitions** — each move is a commitment. Disputes are trivially resolved.
3. **Channel IS the game** — Fiber payment channel doubles as game transport and escrow.
4. **Timeout is handled by a trusted clock server** (MVP) — pragmatic, like lichess. Decentralise later.

---

## Cells

### Game Cell (open game)
```
lock: 2-of-2 multisig (player_a_pubkey, player_b_pubkey)
type: chess-contract
data:
  - player_a_pubkey: [u8; 33]
  - player_b_pubkey: [u8; 33]
  - stake_a: u64          # CKB locked by player A
  - stake_b: u64          # CKB locked by player B
  - time_control: u32     # total seconds per player
  - game_start_block: u64 # block number at open
  - state_hash: [u8; 32]  # Blake2b(board_state)
  - move_count: u32
```

### Closed Game (final state)
Submitted by the winner. Contains the full move log. Contract replays and validates.

---

## Move Format

Each move is a signed message:

```c
typedef struct {
    uint8_t  prev_state_hash[32]; // Blake2b of previous board
    uint8_t  move[5];             // UCI: "e2e4\0" or "e7e8q\0" (promotion)
    uint32_t move_number;         // monotonically increasing, anti-replay
    uint64_t claimed_time_ms;     // player's clock at time of move
    uint8_t  player_pubkey[33];   // signer
    uint8_t  signature[65];       // secp256k1 over above fields
} ChessMove;
```

Move log = array of ChessMove, append-only.

---

## Contract Behaviour

### `open_game`
- Verifies both players have signed
- Records pubkeys, stakes, time control, initial state hash
- Locks funds in game cell

### `close_game` (cooperative)
- Both players sign the final state
- Contract releases funds: winner gets stake_a + stake_b, or split on draw
- Fast path — no replay needed if both sign the outcome

### `dispute_close` (uncooperative)
- One player submits the full move log
- Contract replays all moves using the chess validator
- Verifies each signature
- Determines winner from final board state (checkmate / resign / time forfeit)
- Releases funds accordingly

### `claim_timeout`
- Player A submits proof that player B hasn't moved in N blocks
- N = `time_control_seconds / 6` (approximate, using block time as clock)
- Contract checks block number delta since last move hash was recorded
- If exceeded: A wins by timeout

---

## Chess Validator (C, runs on CKB-VM)

The validator needs to:
1. Parse UCI move notation
2. Maintain board state (8x8, piece positions)
3. Validate piece-specific movement rules (all 6 piece types)
4. Detect check (prevent moving into check)
5. Detect checkmate / stalemate
6. Handle special moves: castling, en passant, pawn promotion

**Source starting point:** kiazamiri/Chess — Windows console C implementation. Needs:
- Strip all Windows-specific code (`<windows.h>`, `gotoxy`, `clrscr`, `conio.h`)
- Strip all display/print code
- Extract pure game logic: board init, move validation, state update
- Fix the coordinate system (the source uses a unusual layout — X steps by 2)
- Add UCI move parser
- Make it work as a pure function: `validate_move(board, move) -> bool`
- Make board state serialisable to bytes (for hashing)

**Estimated: ~400-600 lines of C for the pure validator.**

Key issues in the reference code to fix:
- Uses `abs(X - x) != abs(2 * (Y - y))` — the board is stored with X stepping by 2 (display coords). Normalise to 0-7 for cleaner logic.
- No en passant or castling in the reference — needs adding
- King function in reference only checks knight-moves, not king moves — appears to be checking for check from knights. Needs restructuring.
- No checkmate detection — must add
- Turn validation mixed into piece functions — separate concerns cleanly

---

## Clock / Timeout Design Options

### Option A — Trusted Clock Server (MVP)
A simple server (like lichess) timestamps each move as it relays it. Server signs the move record. Either player submits the server-signed log on-chain. The server's signature is a witness.

**Pros:** Simple, familiar pattern, no clock lies possible
**Cons:** Trust in server; server can censor (but not fake moves — moves are player-signed)

Implementation: lightweight Node.js relay. Players connect, relay timestamps and broadcasts moves. Both players see the same move log with server timestamps.

### Option B — Block-Time Clock
Moves include the CKB block number they were submitted at. The contract reads `ckb_load_header` to verify block timestamps. Coarse (6s resolution) but trustless.

**Pros:** Fully on-chain, no trusted party
**Cons:** 6s granularity — only works for long time controls (5+ min per player)

### Option C — Client Clock, Aggregate Only
Players claim individual move times. Contract only verifies that total claimed time per player ≤ time_control. Players can lie about which moves took long, but can't exceed total budget.

**Pros:** No server, works with short time controls
**Cons:** Players can game the per-move time (doesn't matter much for pure chess, matters for clock-based strategy)

**MVP: Option A (trusted server)**. Build Option B as the trustless upgrade path.

---

## Fiber Integration

Fiber channel provides:
- **Peer connection** — two nodes already connected (payment channel)
- **Signed messages** — both parties already signing channel updates
- **Escrow** — locked CKB in the channel = game stakes

Game moves travel as Fiber channel updates. Each move shifts a tiny amount (1 shannon = 0.00000001 CKB) from the moving player's balance to the other. The amount encodes nothing — it's just a trigger. The actual game state is in the message payload.

When the game ends: cooperative close collapses the channel. Winner takes both stakes. If the Fiber channel is already funded for other purposes, the game stake is just a sub-accounting on top.

**"What if it pretended it wasn't [just payment]"** — the Fiber channel message format allows arbitrary data alongside the payment. The game state hash goes there. Fiber doesn't know it's chess; it just sees signed balance updates with opaque payloads.

---

## Open Questions

1. **Pawn promotion UI** — how does the client communicate promotion piece choice? (Add to move notation: `e7e8q`)
2. **Draw offers** — mutual agreement needed; both players sign a draw message
3. **Resign** — single player signs a resign message; other player uses it to claim funds
4. **Multi-game channels** — one Fiber channel, multiple sequential games. Channel close at end of session.
5. **Rating system** — off-chain, run by the relay server. Trustless rating is a separate problem.
6. **Spectators** — relay server can broadcast the move log publicly (without exposing private keys)

---

## Implementation Order

1. **Chess validator in C** — pure logic, no CKB deps. Test with standard game positions.
2. **CKB script entry point** — wire validator into `entry.c`, load cells, verify signatures
3. **Host tests** — test full game scenarios: normal win, dispute, timeout claim
4. **Rust SDK** — `open_game`, `submit_move`, `close_game`, `dispute` helper functions
5. **Terminal client** — two terminals, relay server, full game end-to-end
6. **Fiber integration** — replace direct TCP with Fiber channel messages
