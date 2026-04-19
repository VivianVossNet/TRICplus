// Copyright (c) 2025-2026 Vivian Voss
// SPDX-License-Identifier: BSD-3-Clause
// Scope: Integration test for the TRIC+ JavaScript/TypeScript bridge.
// Verifies all six primitives against a running server.

import { strict as assert } from 'node:assert';
import { after, before, test } from 'node:test';

import { Connection } from '../src/index.js';

const DEFAULT_SOCKET = '/tmp/tric-js-test/server.sock';
const socketPath = process.env['TRIC_SOCKET'] ?? DEFAULT_SOCKET;

let conn: Connection;

before(() => {
  conn = new Connection(socketPath);
});

after(() => {
  conn.close();
});

test('connection is valid', () => {
  assert.equal(conn.valid(), true);
});

test('read returns written value', async () => {
  await conn.write('test:1', 'hello');
  const value = await conn.read('test:1');
  assert.notEqual(value, null);
});

test('read returns correct length', async () => {
  await conn.write('test:len', 'hello');
  const value = await conn.read('test:len');
  assert.equal(value?.length, 5);
});

test('read returns correct content', async () => {
  await conn.write('test:content', 'hello');
  const value = await conn.read('test:content');
  assert.deepEqual(value, new Uint8Array(Buffer.from('hello')));
});

test('write overwrites', async () => {
  await conn.write('test:over', 'original');
  await conn.write('test:over', 'updated');
  const value = await conn.read('test:over');
  assert.deepEqual(value, new Uint8Array(Buffer.from('updated')));
});

test('delete removes key', async () => {
  await conn.write('test:del', 'payload');
  await conn.del('test:del');
  const value = await conn.read('test:del');
  assert.equal(value, null);
});

test('cad mismatch returns false', async () => {
  await conn.write('test:cas-miss', 'original');
  const matched = await conn.cad('test:cas-miss', 'wrong');
  assert.equal(matched, false);
});

test('cad mismatch keeps value', async () => {
  await conn.write('test:cas-keep', 'original');
  await conn.cad('test:cas-keep', 'wrong');
  const value = await conn.read('test:cas-keep');
  assert.notEqual(value, null);
});

test('cad match returns true', async () => {
  await conn.write('test:cas-match', 'original');
  const matched = await conn.cad('test:cas-match', 'original');
  assert.equal(matched, true);
});

test('cad match deletes', async () => {
  await conn.write('test:cas-del', 'original');
  await conn.cad('test:cas-del', 'original');
  const value = await conn.read('test:cas-del');
  assert.equal(value, null);
});

test('ttl succeeds', async () => {
  await conn.write('test:ttl', 'ephemeral');
  await conn.ttl('test:ttl', 60_000);
});

test('ttl key still readable', async () => {
  await conn.write('test:ttl-read', 'ephemeral');
  await conn.ttl('test:ttl-read', 60_000);
  const value = await conn.read('test:ttl-read');
  assert.notEqual(value, null);
});

test('scan returns results', async () => {
  await conn.write('scan:a', '1');
  await conn.write('scan:b', '2');
  await conn.write('scan:c', '3');
  const pairs = await conn.scan('scan:');
  assert.ok(pairs.length >= 3);
  await conn.del('scan:a');
  await conn.del('scan:b');
  await conn.del('scan:c');
});

test('roundtrip varied bytes', async () => {
  const value = 'value with spaces and more bytes';
  await conn.write('test:slice', value);
  const got = await conn.read('test:slice');
  assert.deepEqual(got, new Uint8Array(Buffer.from(value)));
});
