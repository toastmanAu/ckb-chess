# ckb-chess

On-chain chess using CKB state channels. Each move is a signed state update. The contract validates move legality. Settlement is on-chain.

**Origin:** Conversation between Matt (Nervos) and Phill (Wyltek), 2026-03-02.

## The Core Idea (Matt's design)

Chess maps perfectly onto CKB's cell model:

1. **Open** — both players lock CKB into a channel cell. Game starts.
2. **Move** — each player appends a move to the running game log and signs the new state hash.
3. **Close** — winner submits the final signed state. Contract validates the game, releases funds.
4. **Timeout** — if one player stops responding, the other can claim after a timelock.

No ZK proofs required. CKB-VM can validate chess rules directly — it's just a move legality checker running on RISC-V. Simple, fast, auditable.

## Why CKB-VM, not ZK

> "One thing I see here is it's probably trivial for ckb-vm to verify a game of chess. So we can do this without the zk fanciness that people usually associate with this kind of thing." — Matt

ZK is for hiding computation. Chess games don't need hiding — both players see the full board. CKB-VM can simply replay and validate the move sequence. No prover, no trusted setup, no 130M cycles of Halo2. Just a chess validator in C running on RISC-V.

## The Timeout Problem

> "The only challenge I can think of is a timeout if one player stops responding." — Matt

Chess has a natural answer: **total game time**. Each player gets N minutes for all their moves. Options:

1. **Trusted server** (pragmatic, like lichess) — server timestamps moves, signs the record. Either player can submit the server-signed log on-chain. Dispute → server's record wins.
2. **Client-enforced** — each move includes a claimed timestamp. Contract only checks that total time per player doesn't exceed the limit. Players can lie about individual move times, but the aggregate is bounded.
3. **Median time** — use CKB block timestamps as a coarse clock. Not per-move precision, but enough for long time controls (e.g. 30 min per player = 300 blocks at 6s each).

**MVP choice: trusted server** — fits how lichess works, pragmatic, builds the on-chain mechanics first. Decentralise the clock later.

## The Fiber Angle

> "What if it pretended that it wasn't [just payment]?" — Phill

Fiber payment channel messages are just signed data between two peers. The payment amount IS the commitment — each move shifts 1 microCKB from loser-in-progress to winner-in-progress, with the game state hash embedded in the message. The Fiber channel doubles as:

- **State transport** — game moves travel as payment channel messages
- **Escrow** — locked CKB enforces honest play
- **Settlement** — channel close = game over, funds move to winner

No separate P2P layer needed. Fiber's existing Tentacle connection carries the game.

## State Commitment

> "Needs some method for commitment of what's being signed." — Matt

Each signed move commits to:

```
move_hash = Blake2b(
  prev_state_hash ||  // hash of previous board state
  move_notation   ||  // UCI format: e2e4, e7e5, etc.
  move_number     ||  // anti-replay
  player_pubkey   ||  // who's signing
  timestamp_claim     // player's claimed move time (loosely enforced)
)
```

Both players maintain the same state hash. Disagreement = provable fraud (submit both sigs, contract picks the valid one).

## Contract Architecture

```
chess-contract/
├── src/
│   ├── chess.c          # move validator (adapted from reference implementation)
│   ├── entry.c          # CKB script entry point
│   └── types.h          # board state, move encoding
```

The contract only runs when:
- **Opening** a game (lock funds, record both pubkeys and time control)
- **Closing** a game (validate full move history, release funds to winner)
- **Claiming timeout** (verify N blocks have passed since last move)

Off-chain: moves are exchanged directly P2P (via Fiber or a relay). Only the final state hits the chain.

## Move Encoding

Standard UCI notation fits in 5 bytes: `e2e4` (from-square, to-square) + 1 byte for promotion piece. Compact, parseable by the on-chain validator.

Full game history for a 40-move game: ~200 bytes. Trivial for a CKB cell.

## Project Structure

```
ckb-chess/
├── contracts/
│   └── chess-contract/     # CKB RISC-V script (C)
├── sdk/
│   └── src/                # Rust SDK: open/move/close/dispute helpers
├── client/                 # Simple terminal client (proof of concept)
├── docs/
│   ├── design.md           # Full protocol spec
│   └── matt-notes.md       # Original design conversation
└── README.md
```

## Status

🚧 **Scaffolding** — design phase. Contract implementation next.

## Related

- [Kabletop](https://github.com/cryptape/kabletop-contracts) — Cryptape's prior art: card game on CKB channels (Lua VM for game logic)
- [ckb-embedded-research: Fiber Gaming Protocol](https://github.com/toastmanAu/ckb-embedded-research/blob/master/embedded-networking/fiber-gaming-protocol.md)
- [Nervos Fiber Network](https://github.com/nervosnetwork/fiber)
