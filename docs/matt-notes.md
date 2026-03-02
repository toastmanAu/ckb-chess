# Matt's Design Notes — 2026-03-02

Matt is head of Nervos (effectively). These are his unfiltered thoughts on on-chain chess, captured as-is.

> It seems easy enough that each player appends a move to the running game and signs it

The append-and-sign model. CKB cells are immutable — a game is just a chain of signed state transitions. Clean.

> The only challenge I can think of is a timeout if one player stops responding

The core hard problem in any on-chain game. Chess has a natural answer: total game time clock.

> One advantage to chess is there is a total game time

This is the key. Unlike arbitrary turn-based games, chess has a built-in time commitment mechanism. You can't stall forever — you have a clock. The contract can enforce total time even if it can't verify per-move times precisely.

> So there could be a mechanism activated like 30 min after the game started

A global game timeout as a backstop. Even without a per-move clock, you can say: if the game isn't finished 30 minutes after opening, the player whose turn it is loses. Coarse, but enforceable on-chain using block numbers.

> Fiber can handle real time pretty well it would seem

Phill's observation. Fiber's Tentacle P2P is a low-latency transport — moves can travel in milliseconds over an existing channel connection.

> It's only payment though

Matt's correct pushback. Fiber is a payment channel protocol. It doesn't natively carry arbitrary game state. You'd be using it as a transport it wasn't designed for.

> What if it pretended that it wasn't?

Phill's lateral move. Fiber channel updates are signed messages. The "payment" is 1 shannon. The payload carries the game state hash. From Fiber's perspective it's a payment. From the game's perspective it's a move commitment. The channel doesn't need to know the difference.

> Needs some method for commitment of what's being signed

Crucial. Both players must sign the same thing. The commitment is: `Blake2b(prev_state_hash || move_notation || move_number || player_pubkey || timestamp)`. Deterministic, verifiable, replay-resistant.

> One thing I see here is it's probably trivial for ckb-vm to verify a game of chess

This is the architectural insight. People reach for ZK when they want "verifiable computation". But ZK hides the computation. Chess doesn't need hiding — both players see the board. CKB-VM just needs to replay the moves and check legality. A chess validator in C is ~500 lines. Trivial for CKB-VM.

> So we can do this without the zk fanciness that people usually associate with this kind of thing

ZK is for: privacy (hide inputs), succinctness (compress huge computation), off-chain computation that's too expensive on-chain. Chess needs: none of the above. CKB-VM handles it directly.

> But we still have to handle the turn-based stuff and timeout

Yes — and this is where the trusted server (lichess model) is pragmatic. The server timestamps moves, but can't fake moves (those are player-signed). The server's role is: relay + clock. Trustless upgrade: block-time clock (6s resolution, enough for long games).

> Ideally we want a chess clock but I think that can be verified by each client, It's hard to prove if someone is lying though

Exactly. Client-side clocks can't be proven to the contract. What CAN be proven: total accumulated time (if each move includes a claimed timestamp, the contract can enforce total ≤ budget but can't enforce individual move times).

> Maybe a trusted server is the way to go then, It fits with like lichess now

Lichess model: open-source server, community trust, not cryptographic trust. Same pragmatism applies here. Build the on-chain mechanics first; the clock can be a trusted relay initially.

> Feel free at any time to disregard my messages, i have gotten in the habit of just sharing things as they come to mind

Don't disregard. This is exactly the kind of unfiltered thinking that produces real designs. Every point Matt raised maps to a concrete protocol decision.

> Feel like there are so many things in my head that are better served by more people knowing

The value of seeding builders. The ideas don't need to be fully formed — they just need to land somewhere fertile.

---

## Key Takeaways for Implementation

| Matt's observation | Implementation decision |
|---|---|
| Append-and-sign move log | `ChessMove[]` array, each with player sig over state hash |
| Total game time timeout | Block-number delta since game open; `claim_timeout` tx after N blocks |
| Fiber as transport | 1 shannon payment per move, game state hash in payload |
| Commitment to what's signed | `Blake2b(prev_hash \|\| move \|\| seq \|\| pubkey \|\| timestamp)` |
| CKB-VM validates chess | C chess validator in the contract, replays full move log on dispute |
| No ZK needed | Direct replay validation — chess is transparent by nature |
| Trusted server for clock | Relay server timestamps + signs move records. Trustless upgrade = block time |
