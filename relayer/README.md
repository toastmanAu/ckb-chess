# ckb-chess relayer

Trusted relayer for ckb-chess, implementing the Fiber turn-based game framework
designed by the CKB DevRel team.

## What it does

- **Message forwarding** — receives signed moves from players, forwards to opponent
- **Challenge coordination** — detects stalls, manages 5-minute challenge windows
- **Preimage custody** — holds both players' payment preimages until game resolves
- **Settlement trigger** — reveals winner's preimage via Fiber RPC when game ends

## Run

```bash
npm install
node src/index.js
```

Environment variables:
```
PORT=8765          # WebSocket port (status HTTP on PORT+1)
CHALLENGE_MS=300000 # Challenge window in ms (default 5 min)
FIBER_RPC_URL=http://127.0.0.1:8227
```

## Protocol

WebSocket JSON messages.

**Client → Relayer:**
```json
{ "type": "join",      "gameId": "ABC123", "pubkey": "03...", "invoiceHash": "..." }
{ "type": "move",      "gameId": "ABC123", "uci": "e2e4", "sig": "...", "timestamp": 1234 }
{ "type": "challenge", "gameId": "ABC123" }
{ "type": "ping" }
```

**Relayer → Client:**
```json
{ "type": "joined",              "gameId": "ABC123", "role": "A" }
{ "type": "opponent_joined",     "gameId": "ABC123", "pubkey": "03..." }
{ "type": "move",                "seq": 1, "uci": "e2e4", "stateHash": "..." }
{ "type": "challenge_started",   "deadline": 1234567890 }
{ "type": "game_over",           "winner": "A", "reason": "challenge_timeout", "moves": [...] }
{ "type": "opponent_disconnected" }
{ "type": "error",               "message": "..." }
```

## Status

`GET http://localhost:8766/status` — returns active games and client count.

## Architecture

```
Player A ─── WS ───┐
                   ├── Relayer ── Fiber RPC (new_invoice / settle_invoice)
Player B ─── WS ───┘         └── CKB RPC  (challenge contract submission)
```

## Reference

[A Universal Turn-Based Competition Framework Based on Fiber](https://ckb-devrel.notion.site/A-Universal-Turn-Based-Competition-Framework-Based-on-Fiber-6309f5bf38208288ae938158afe27343)
