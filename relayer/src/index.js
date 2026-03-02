/**
 * ckb-chess relayer
 *
 * Implements the trusted relayer role from Matt's Universal Turn-Based
 * Competition Framework (https://ckb-devrel.notion.site/...)
 *
 * Responsibilities:
 *   1. Message forwarding — receive signed moves from players, forward to opponent
 *   2. Challenge coordination — detect stalls, manage challenge windows
 *   3. Preimage custody — hold both players' payment preimages until game end
 *   4. Chain monitoring — watch CKB for challenge contract submissions
 *   5. Settlement — reveal winning player's preimage on-chain when game resolves
 *
 * Protocol (WebSocket, JSON messages):
 *
 *   Client → Relayer:
 *     { type: "join",  gameId, pubkey, invoiceHash }
 *     { type: "move",  gameId, uci, sig, timestamp }
 *     { type: "challenge", gameId }        // report opponent timeout
 *     { type: "ping" }
 *
 *   Relayer → Client:
 *     { type: "joined",   gameId, role: "A"|"B" }
 *     { type: "opponent_joined", gameId }
 *     { type: "move",     gameId, seq, uci, sig, stateHash, timestamp }
 *     { type: "challenge_started", gameId, deadline }
 *     { type: "game_over", gameId, winner: "A"|"B", reason }
 *     { type: "error",    message }
 *     { type: "pong" }
 */

'use strict';

const { WebSocketServer } = require('ws');
const { Game, GameState } = require('./game');
const { FiberRPC }        = require('./fiber');

const PORT            = parseInt(process.env.PORT || '8765');
const CHALLENGE_MS    = parseInt(process.env.CHALLENGE_MS || String(5 * 60 * 1000));
const FIBER_RPC_URL   = process.env.FIBER_RPC_URL || 'http://127.0.0.1:8227';

const games    = new Map(); // gameId → Game
const sessions = new Map(); // ws → { gameId, role }
const fiber    = new FiberRPC(FIBER_RPC_URL);

// ── Helpers ──────────────────────────────────────────────────────────────────

function send(ws, msg) {
  if (ws.readyState === ws.OPEN) {
    ws.send(JSON.stringify(msg));
  }
}

function getOpponent(game, role) {
  const player = role === 'A' ? game.playerB : game.playerA;
  return player?.ws;
}

function broadcast(game, msg, exclude = null) {
  for (const ws of [game.playerA?.ws, game.playerB?.ws]) {
    if (ws && ws !== exclude) send(ws, msg);
  }
}

function makeGameId() {
  return Math.random().toString(36).slice(2, 10).toUpperCase();
}

// ── Message handlers ─────────────────────────────────────────────────────────

function handleJoin(ws, msg) {
  const { gameId, pubkey, invoiceHash } = msg;

  if (!pubkey) return send(ws, { type: 'error', message: 'pubkey required' });

  let game = gameId ? games.get(gameId) : null;

  if (!game) {
    // Create new game — this player is A (white, moves first)
    const id = gameId || makeGameId();
    game     = new Game(id, { pubkey, invoiceHash });
    games.set(id, game);
    game.playerA.ws = ws;
    sessions.set(ws, { gameId: id, role: 'A' });
    send(ws, { type: 'joined', gameId: id, role: 'A' });
    console.log(`[${id}] Game created by ${pubkey.slice(0,12)}...`);
  } else if (game.state === GameState.WAITING) {
    // Second player joins — player B (black)
    if (game.playerA.pubkey === pubkey) {
      return send(ws, { type: 'error', message: 'cannot play against yourself' });
    }
    game.addPlayer({ pubkey, invoiceHash });
    game.playerB.ws = ws;
    sessions.set(ws, { gameId: game.id, role: 'B' });
    send(ws, { type: 'joined', gameId: game.id, role: 'B' });
    // Notify player A their opponent has arrived
    send(game.playerA.ws, { type: 'opponent_joined', gameId: game.id, pubkey });
    console.log(`[${game.id}] Player B joined: ${pubkey.slice(0,12)}...`);
  } else {
    send(ws, { type: 'error', message: 'game not available' });
  }
}

function handleMove(ws, msg) {
  const session = sessions.get(ws);
  if (!session) return send(ws, { type: 'error', message: 'not in a game' });

  const { gameId, role } = session;
  const game = games.get(gameId);
  if (!game) return send(ws, { type: 'error', message: 'game not found' });

  const { uci, sig, timestamp } = msg;
  if (!uci || !sig) return send(ws, { type: 'error', message: 'uci and sig required' });

  const moverPubkey = role === 'A' ? game.playerA.pubkey : game.playerB.pubkey;
  const result      = game.addMove(uci, moverPubkey, sig, timestamp || Date.now());

  if (!result.ok) {
    return send(ws, { type: 'error', message: result.error });
  }

  console.log(`[${gameId}] Move ${result.seq}: ${uci} by ${role}`);

  // Forward the move to opponent
  const opponentWs = getOpponent(game, role);
  const moveMsg    = {
    type:      'move',
    gameId,
    seq:       result.seq,
    uci,
    sig,
    stateHash: result.stateHash,
    timestamp: timestamp || Date.now(),
  };

  if (opponentWs) {
    send(opponentWs, moveMsg);
  }

  // Echo ack back to sender with state hash
  send(ws, { type: 'move_ack', seq: result.seq, stateHash: result.stateHash });
}

function handleChallenge(ws, msg) {
  const session = sessions.get(ws);
  if (!session) return send(ws, { type: 'error', message: 'not in a game' });

  const game = games.get(session.gameId);
  if (!game) return send(ws, { type: 'error', message: 'game not found' });

  const result = game.initiateChallenge(session.role, CHALLENGE_MS);
  if (!result.ok) return send(ws, { type: 'error', message: result.error });

  console.log(`[${game.id}] Challenge initiated by ${session.role}, deadline ${new Date(result.deadline).toISOString()}`);

  broadcast(game, {
    type:      'challenge_started',
    gameId:    game.id,
    initiator: session.role,
    deadline:  result.deadline,
  });

  // Set a timer — if challenge window expires without response, declare winner
  setTimeout(() => {
    if (game.state !== GameState.CHALLENGE) return;
    // Initiator wins — opponent failed to respond
    game.state = GameState.COMPLETE;
    const winner = session.role;
    console.log(`[${game.id}] Challenge expired — ${winner} wins`);
    broadcast(game, {
      type:   'game_over',
      gameId: game.id,
      winner,
      reason: 'challenge_timeout',
      moves:  game.getMoveSet(),
    });
    // TODO: trigger on-chain settlement via Fiber RPC
  }, CHALLENGE_MS);
}

// ── WebSocket server ─────────────────────────────────────────────────────────

const wss = new WebSocketServer({ port: PORT });

wss.on('connection', (ws, req) => {
  const ip = req.socket.remoteAddress;
  console.log(`[relayer] Client connected: ${ip}`);

  ws.on('message', raw => {
    let msg;
    try { msg = JSON.parse(raw); }
    catch { return send(ws, { type: 'error', message: 'invalid JSON' }); }

    switch (msg.type) {
      case 'join':      handleJoin(ws, msg);      break;
      case 'move':      handleMove(ws, msg);      break;
      case 'challenge': handleChallenge(ws, msg); break;
      case 'ping':      send(ws, { type: 'pong' }); break;
      default:
        send(ws, { type: 'error', message: `unknown message type: ${msg.type}` });
    }
  });

  ws.on('close', () => {
    const session = sessions.get(ws);
    if (session) {
      const game = games.get(session.gameId);
      if (game && game.state === GameState.ACTIVE) {
        console.log(`[${session.gameId}] Player ${session.role} disconnected`);
        // Notify opponent — they can initiate challenge
        const opponentWs = getOpponent(game, session.role);
        if (opponentWs) {
          send(opponentWs, {
            type:   'opponent_disconnected',
            gameId: session.gameId,
            tip:    'Send { type: "challenge" } to start the challenge window',
          });
        }
      }
      sessions.delete(ws);
    }
    console.log(`[relayer] Client disconnected: ${ip}`);
  });

  ws.on('error', err => console.error(`[relayer] WS error: ${err.message}`));
});

wss.on('listening', () => {
  console.log(`ckb-chess relayer listening on ws://0.0.0.0:${PORT}`);
  console.log(`Fiber RPC: ${FIBER_RPC_URL}`);
  console.log(`Challenge window: ${CHALLENGE_MS / 1000}s`);
});

// ── Status endpoint (HTTP on PORT+1) ────────────────────────────────────────
const http = require('http');
http.createServer((req, res) => {
  if (req.url === '/status') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      games:   [...games.values()].map(g => g.toSummary()),
      clients: wss.clients.size,
      uptime:  process.uptime(),
    }));
  } else {
    res.writeHead(404);
    res.end();
  }
}).listen(PORT + 1, () => {
  console.log(`Status: http://0.0.0.0:${PORT + 1}/status`);
});
