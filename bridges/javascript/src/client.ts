// Copyright (c) 2025-2026 Vivian Voss
// SPDX-License-Identifier: BSD-3-Clause
// Scope: TRIC+ JavaScript/TypeScript client.
// Speaks the TRIC+ wire protocol over UDS DGRAM via the unix-dgram npm addon.

import { createRequire } from 'node:module';
import { unlinkSync } from 'node:fs';

const require_ = createRequire(import.meta.url);
// eslint-disable-next-line @typescript-eslint/no-require-imports
const unix = require_('unix-dgram') as UnixDgramModule;

interface UnixDgramSocket {
  bind(path: string): void;
  connect(path: string): void;
  send(buffer: Buffer, callback?: (err: Error | null) => void): void;
  close(): void;
  on(event: 'message', listener: (buf: Buffer) => void): this;
  on(event: 'error', listener: (err: Error) => void): this;
  on(event: 'congestion', listener: () => void): this;
  on(event: 'writable', listener: () => void): this;
}

interface UnixDgramModule {
  createSocket(type: 'unix_dgram'): UnixDgramSocket;
}

const OP_READ = 0x01;
const OP_WRITE = 0x02;
const OP_DELETE = 0x03;
const OP_CAD = 0x04;
const OP_TTL = 0x05;
const OP_SCAN = 0x06;

const RESP_OK = 0x80;
const RESP_OK_DATA = 0x81;
const RESP_SCAN_CHUNK = 0x90;
const RESP_SCAN_END = 0x91;

export class TricError extends Error {
  override readonly name = 'TricError';
}

function toBuffer(value: string | Uint8Array): Buffer {
  if (typeof value === 'string') {
    return Buffer.from(value, 'utf8');
  }
  return Buffer.from(value);
}

interface PendingReply {
  resolve: (buf: Buffer) => void;
  reject: (err: Error) => void;
}

export class Connection {
  private readonly socket: UnixDgramSocket;
  private readonly clientPath: string;
  private requestId = 1;
  private owned = false;
  private inbox: Buffer[] = [];
  private waiters: PendingReply[] = [];
  private scanWaiter: ((buf: Buffer) => void) | null = null;

  constructor(socketPath: string) {
    this.clientPath = `/tmp/tric-js-${process.pid}.sock`;
    try {
      unlinkSync(this.clientPath);
    } catch {
      // file did not exist; nothing to clean up
    }
    this.socket = unix.createSocket('unix_dgram');
    this.socket.on('message', (buf) => {
      if (this.scanWaiter) {
        this.scanWaiter(buf);
        return;
      }
      const waiter = this.waiters.shift();
      if (waiter) {
        waiter.resolve(buf);
      } else {
        this.inbox.push(buf);
      }
    });
    this.socket.on('error', (err) => {
      while (this.waiters.length > 0) {
        const waiter = this.waiters.shift();
        waiter?.reject(err);
      }
    });
    try {
      this.socket.bind(this.clientPath);
      this.socket.connect(socketPath);
      this.owned = true;
    } catch (err) {
      this.socket.close();
      throw new TricError(`connect failed: ${(err as Error).message}`);
    }
  }

  close(): void {
    if (this.owned) {
      this.socket.close();
      this.owned = false;
      try {
        unlinkSync(this.clientPath);
      } catch {
        // already unlinked
      }
    }
  }

  valid(): boolean {
    return this.owned;
  }

  private nextRequestId(): number {
    const rid = this.requestId;
    this.requestId = (this.requestId + 1) >>> 0;
    return rid;
  }

  private awaitReply(): Promise<Buffer> {
    const pending = this.inbox.shift();
    if (pending !== undefined) {
      return Promise.resolve(pending);
    }
    return new Promise<Buffer>((resolve, reject) => {
      this.waiters.push({ resolve, reject });
    });
  }

  private sendDatagram(data: Buffer): Promise<void> {
    return new Promise((resolve, reject) => {
      this.socket.send(data, (err) => {
        if (err) {
          reject(new TricError(`send failed: ${err.message}`));
        } else {
          resolve();
        }
      });
    });
  }

  private async roundtrip(opcode: number, payload: Buffer): Promise<{ op: number; body: Buffer }> {
    const rid = this.nextRequestId();
    const header = Buffer.alloc(5);
    header.writeUInt32BE(rid, 0);
    header.writeUInt8(opcode, 4);
    const datagram = Buffer.concat([header, payload]);
    await this.sendDatagram(datagram);
    const reply = await this.awaitReply();
    if (reply.length < 5) {
      throw new TricError('malformed response');
    }
    const replyRid = reply.readUInt32BE(0);
    if (replyRid !== rid) {
      throw new TricError(`request-id mismatch: sent ${rid}, got ${replyRid}`);
    }
    return { op: reply.readUInt8(4), body: reply.subarray(5) };
  }

  async read(key: string | Uint8Array): Promise<Uint8Array | null> {
    const keyBuf = toBuffer(key);
    const payload = Buffer.concat([this.lengthPrefix(keyBuf.length), keyBuf]);
    const { op, body } = await this.roundtrip(OP_READ, payload);
    if (op === RESP_OK_DATA) {
      if (body.length < 4) {
        throw new TricError('malformed OK_DATA response');
      }
      const valueLen = body.readUInt32BE(0);
      return Uint8Array.from(body.subarray(4, 4 + valueLen));
    }
    return null;
  }

  async write(key: string | Uint8Array, value: string | Uint8Array, durationMs = 0): Promise<void> {
    const keyBuf = toBuffer(key);
    const valueBuf = toBuffer(value);
    const durationBuf = Buffer.alloc(8);
    durationBuf.writeBigUInt64BE(BigInt(durationMs), 0);
    const payload = Buffer.concat([
      this.lengthPrefix(keyBuf.length),
      keyBuf,
      this.lengthPrefix(valueBuf.length),
      valueBuf,
      durationBuf,
    ]);
    const { op } = await this.roundtrip(OP_WRITE, payload);
    if (op !== RESP_OK) {
      throw new TricError(`write failed, opcode 0x${op.toString(16)}`);
    }
  }

  async del(key: string | Uint8Array): Promise<void> {
    const keyBuf = toBuffer(key);
    const payload = Buffer.concat([this.lengthPrefix(keyBuf.length), keyBuf]);
    const { op } = await this.roundtrip(OP_DELETE, payload);
    if (op !== RESP_OK) {
      throw new TricError(`del failed, opcode 0x${op.toString(16)}`);
    }
  }

  async cad(key: string | Uint8Array, expected: string | Uint8Array): Promise<boolean> {
    const keyBuf = toBuffer(key);
    const expectedBuf = toBuffer(expected);
    const payload = Buffer.concat([
      this.lengthPrefix(keyBuf.length),
      keyBuf,
      this.lengthPrefix(expectedBuf.length),
      expectedBuf,
    ]);
    const { op, body } = await this.roundtrip(OP_CAD, payload);
    return op === RESP_OK_DATA && body.length >= 1 && body.readUInt8(0) === 0x01;
  }

  async ttl(key: string | Uint8Array, durationMs: number): Promise<void> {
    const keyBuf = toBuffer(key);
    const durationBuf = Buffer.alloc(8);
    durationBuf.writeBigUInt64BE(BigInt(durationMs), 0);
    const payload = Buffer.concat([this.lengthPrefix(keyBuf.length), keyBuf, durationBuf]);
    const { op } = await this.roundtrip(OP_TTL, payload);
    if (op !== RESP_OK) {
      throw new TricError(`ttl failed, opcode 0x${op.toString(16)}`);
    }
  }

  async scan(prefix: string | Uint8Array): Promise<Array<[Uint8Array, Uint8Array]>> {
    const prefixBuf = toBuffer(prefix);
    const payload = Buffer.concat([this.lengthPrefix(prefixBuf.length), prefixBuf]);
    const rid = this.nextRequestId();
    const header = Buffer.alloc(5);
    header.writeUInt32BE(rid, 0);
    header.writeUInt8(OP_SCAN, 4);
    const datagram = Buffer.concat([header, payload]);
    const pairs: Array<[Uint8Array, Uint8Array]> = [];
    const chunks: Buffer[] = [];
    const done = new Promise<void>((resolve, reject) => {
      this.scanWaiter = (buf: Buffer) => {
        if (buf.length < 5) {
          this.scanWaiter = null;
          reject(new TricError('malformed scan reply'));
          return;
        }
        const replyRid = buf.readUInt32BE(0);
        if (replyRid !== rid) {
          this.scanWaiter = null;
          reject(new TricError(`scan request-id mismatch: sent ${rid}, got ${replyRid}`));
          return;
        }
        const op = buf.readUInt8(4);
        if (op === RESP_SCAN_END) {
          this.scanWaiter = null;
          resolve();
          return;
        }
        if (op !== RESP_SCAN_CHUNK) {
          this.scanWaiter = null;
          reject(new TricError(`scan unexpected opcode 0x${op.toString(16)}`));
          return;
        }
        chunks.push(buf);
      };
    });
    await this.sendDatagram(datagram);
    await done;
    for (const buf of chunks) {
      const body = buf.subarray(5);
      let offset = 4;
      const keyLen = body.readUInt32BE(offset);
      offset += 4;
      const keyBytes = Uint8Array.from(body.subarray(offset, offset + keyLen));
      offset += keyLen;
      const valueLen = body.readUInt32BE(offset);
      offset += 4;
      const valueBytes = Uint8Array.from(body.subarray(offset, offset + valueLen));
      pairs.push([keyBytes, valueBytes]);
    }
    return pairs;
  }

  private lengthPrefix(length: number): Buffer {
    const buf = Buffer.alloc(4);
    buf.writeUInt32BE(length, 0);
    return buf;
  }
}
