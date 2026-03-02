/**
 * fiber.js — Fiber RPC client
 *
 * Thin wrapper around the Fiber JSON-RPC interface.
 * Docs: https://github.com/nervosnetwork/fiber/blob/v0.7.0/crates/fiber-lib/src/rpc/README.md
 *
 * Used by the relayer to:
 *  - Create invoices (new_invoice) for asset locking
 *  - Settle invoices (settle_invoice) when preimage is revealed
 *  - Monitor channel state
 */

'use strict';

const http = require('http');

class FiberRPC {
  constructor(url = 'http://127.0.0.1:8227') {
    this.url  = url;
    this._id  = 1;
  }

  /**
   * call — make a raw JSON-RPC call
   */
  async call(method, params = []) {
    const body = JSON.stringify({
      jsonrpc: '2.0',
      id:      this._id++,
      method,
      params,
    });

    return new Promise((resolve, reject) => {
      const url  = new URL(this.url);
      const req  = http.request({
        hostname: url.hostname,
        port:     url.port || 8227,
        path:     url.pathname || '/',
        method:   'POST',
        headers:  {
          'Content-Type':   'application/json',
          'Content-Length': Buffer.byteLength(body),
        },
      }, res => {
        let data = '';
        res.on('data', chunk => data += chunk);
        res.on('end', () => {
          try {
            const parsed = JSON.parse(data);
            if (parsed.error) reject(new Error(`Fiber RPC error: ${JSON.stringify(parsed.error)}`));
            else resolve(parsed.result);
          } catch (e) {
            reject(e);
          }
        });
      });
      req.on('error', reject);
      req.write(body);
      req.end();
    });
  }

  /**
   * newInvoice — create a PayToPaymentHash invoice
   * amount: shannons (BigInt or string)
   * paymentPreimage: 32-byte hex (the preimage — keep secret until reveal)
   * Returns invoice object including payment_hash
   */
  async newInvoice(amount, paymentPreimage, expirySeconds = 3600) {
    return this.call('new_invoice', [{
      amount:          amount.toString(),
      payment_preimage: paymentPreimage,
      expiry:          expirySeconds,
      currency:        'Fibt', // Fiber testnet; use 'Fibb' for mainnet
      description:     'ckb-chess game stake',
    }]);
  }

  /**
   * settleInvoice — reveal preimage to settle a pending invoice
   * paymentPreimage: 32-byte hex
   */
  async settleInvoice(paymentPreimage) {
    return this.call('settle_invoice', [{ payment_preimage: paymentPreimage }]);
  }

  /**
   * sendPayment — send payment to a payment hash
   * paymentHash: 32-byte hex
   */
  async sendPayment(paymentHash, amount) {
    return this.call('send_payment', [{
      payment_hash: paymentHash,
      amount:       amount.toString(),
    }]);
  }

  /**
   * getChannel — get channel info
   */
  async getChannel(channelId) {
    return this.call('get_channel', [{ channel_id: channelId }]);
  }

  /**
   * listChannels — list all channels
   */
  async listChannels() {
    return this.call('list_channels', [{}]);
  }

  /**
   * nodeInfo — get local node info (pubkey, addresses)
   */
  async nodeInfo() {
    return this.call('node_info', []);
  }
}

module.exports = { FiberRPC };
