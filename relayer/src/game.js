/**
 * game.js — Game session state management
 *
 * A Game tracks two players, their move history, signatures,
 * preimage hashes, and challenge state.
 *
 * Per Matt's framework:
 *   - Each move is signed by the moving player
 *   - State hash = Blake2b(prev_hash || uci_move || seq || pubkey || timestamp)
 *   - Relayer stores move + sig pairs, forwards to opponent
 *   - On challenge: relayer submits full move set to CKB contract
 */

'use strict';

const crypto = require('crypto');

const GameState = {
  WAITING:    'waiting',     // waiting for second player
  ACTIVE:     'active',      // game in progress
  CHALLENGE:  'challenge',   // challenge window open
  COMPLETE:   'complete',    // winner determined
  ABANDONED:  'abandoned',   // both players disconnected
};

class Game {
  constructor(gameId, playerA) {
    this.id         = gameId;
    this.state      = GameState.WAITING;
    this.createdAt  = Date.now();
    this.updatedAt  = Date.now();

    // Players: { pubkey, address, ws, invoiceHash, preimage }
    this.playerA    = { ...playerA, ws: null, invoiceHash: null };
    this.playerB    = null;

    // Move log: [{ seq, uci, pubkey, sig, stateHash, timestamp }]
    this.moves      = [];

    // Whose turn: 'A' | 'B'  (A always plays white, moves first)
    this.turn       = 'A';

    // Challenge state
    this.challenge  = null; // { initiator, deadline, submittedAt }

    // Fiber channel IDs (set when channels open)
    this.channelA   = null;
    this.channelB   = null;
  }

  addPlayer(playerB) {
    this.playerB = { ...playerB, ws: null, invoiceHash: null };
    this.state   = GameState.ACTIVE;
    this.updatedAt = Date.now();
  }

  /**
   * addMove — validate and record a signed move
   * Returns { ok, error }
   */
  addMove(uci, pubkey, sig, timestamp) {
    if (this.state !== GameState.ACTIVE) {
      return { ok: false, error: 'game not active' };
    }

    // Verify it's this player's turn
    const mover = this.turn === 'A' ? this.playerA : this.playerB;
    if (mover.pubkey !== pubkey) {
      return { ok: false, error: 'not your turn' };
    }

    const seq        = this.moves.length;
    const prevHash   = seq === 0
      ? '0'.repeat(64)
      : this.moves[seq - 1].stateHash;

    const stateHash  = this._computeStateHash(prevHash, uci, seq, pubkey, timestamp);

    this.moves.push({ seq, uci, pubkey, sig, stateHash, timestamp });
    this.turn      = this.turn === 'A' ? 'B' : 'A';
    this.updatedAt = Date.now();

    return { ok: true, seq, stateHash };
  }

  /**
   * initiateChallenge — called when a player stops responding
   * challengeWindowMs: how long opponent has to resume (default 5 min)
   */
  initiateChallenge(initiator, challengeWindowMs = 5 * 60 * 1000) {
    if (this.state !== GameState.ACTIVE) {
      return { ok: false, error: 'cannot challenge in current state' };
    }
    this.state     = GameState.CHALLENGE;
    this.challenge = {
      initiator,
      deadline:    Date.now() + challengeWindowMs,
      submittedAt: null,
    };
    this.updatedAt = Date.now();
    return { ok: true, deadline: this.challenge.deadline };
  }

  /**
   * getMoveSet — serialise full move history for CKB contract submission
   * Format matches chess.c's replay input
   */
  getMoveSet() {
    return this.moves.map(m => ({
      seq:       m.seq,
      uci:       m.uci,
      pubkey:    m.pubkey,
      sig:       m.sig,
      stateHash: m.stateHash,
      timestamp: m.timestamp,
    }));
  }

  toSummary() {
    return {
      id:        this.id,
      state:     this.state,
      moveCount: this.moves.length,
      turn:      this.turn,
      playerA:   this.playerA?.pubkey?.slice(0, 12) + '...',
      playerB:   this.playerB?.pubkey?.slice(0, 12) + '...',
      createdAt: this.createdAt,
      updatedAt: this.updatedAt,
    };
  }

  _computeStateHash(prevHash, uci, seq, pubkey, timestamp) {
    // Blake2b not in Node stdlib — use SHA256 as placeholder until
    // we add the blake2b npm package. Same structure, easy swap.
    const input = `${prevHash}:${uci}:${seq}:${pubkey}:${timestamp}`;
    return crypto.createHash('sha256').update(input).digest('hex');
  }
}

module.exports = { Game, GameState };
