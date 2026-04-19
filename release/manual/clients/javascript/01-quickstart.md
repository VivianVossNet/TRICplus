# JavaScript and TypeScript Bridge: Quickstart

The TRIC+ JavaScript and TypeScript client is one TypeScript codebase compiled to JavaScript and shipped with `.d.ts` declarations. JavaScript projects `import { Connection } from 'tric'` and get a runtime; TypeScript projects get the same code plus typed access. Node.js 20 or later, Unix-domain datagram socket via the `unix-dgram` npm addon. Permutive routing stays on the server: a `write` with a `durationMs` above zero lives in the `BTreeMap`; a plain `write` lives in SQLite; the client sees one `Connection` class with six primitives.

## Requirements

- **Node.js 20+**
- macOS, Linux, or FreeBSD (Windows is out of scope for the UDS-DGRAM transport)
- A running TRIC+ server reachable via a Unix-domain socket

## Integration

The package is published locally at `bridges/javascript/`. In a consumer project:

```bash
npm install /path/to/TRIC/bridges/javascript
```

This installs `tric` as a dependency and compiles the `unix-dgram` native addon as part of the install. Then:

```js
import { Connection, TricError } from 'tric';
```

TypeScript consumers get the same import with full type support from the bundled `.d.ts` declarations.

## Connect

```js
import { Connection } from 'tric';

const conn = new Connection('/var/run/tric/server.sock');
try {
  if (!conn.valid()) {
    throw new Error('connect failed');
  }
  // ... use the connection ...
} finally {
  conn.close();
}
```

The constructor binds a temporary client socket at `/tmp/tric-js-{pid}.sock` and connects to the server. `close()` releases the socket and removes the temporary path.

## Primitives

All primitive methods are async and return Promises. Use `await` or `.then()`.

### Write and read

```js
await conn.write('user:42', 'alice');

const value = await conn.read('user:42');
if (value !== null) {
  console.log(Buffer.from(value).toString()); // 'alice'
}
```

`read` resolves to `Uint8Array | null`. `null` means the key does not exist, or the read failed; the bridge does not distinguish the two cases. Both `write` and `read` accept `string | Uint8Array`. Strings are UTF-8 encoded; returns are always `Uint8Array`.

### Delete

```js
await conn.del('user:42');
```

Rejects with `TricError` on socket failure. Deleting a missing key succeeds silently. The method is named `del` rather than `delete` because `delete` is a reserved word in JavaScript.

### Compare-and-delete

```js
const matched = await conn.cad('job:1', 'pending');
// matched === true:  value was 'pending', key is now deleted
// matched === false: value was something else, key is untouched
```

Atomic. Resolves to a boolean.

### TTL

```js
await conn.write('session:abc', 'token');
await conn.ttl('session:abc', 3_600_000);
```

Duration in milliseconds. Missing key is a silent no-op.

### Prefix scan

```js
const pairs = await conn.scan('user:');
for (const [key, value] of pairs) {
  console.log(Buffer.from(key).toString(), '=', Buffer.from(value).toString());
}
```

Resolves to `Array<[Uint8Array, Uint8Array]>`. Decode either element with `Buffer.from(...).toString()` for UTF-8 content, or work with raw bytes.

## API

| Method | Signature | Purpose |
|--------|-----------|---------|
| `new Connection(path)` | `(string) => Connection` | Open a UDS DGRAM connection |
| `close()` | `() => void` | Close the socket |
| `valid()` | `() => boolean` | Is the connection open |
| `read(key)` | `(string \| Uint8Array) => Promise<Uint8Array \| null>` | Fetch a value |
| `write(key, value, durationMs?)` | `(string \| Uint8Array, string \| Uint8Array, number?) => Promise<void>` | Store a value |
| `del(key)` | `(string \| Uint8Array) => Promise<void>` | Remove a key |
| `cad(key, expected)` | `(string \| Uint8Array, string \| Uint8Array) => Promise<boolean>` | Atomic compare-and-delete |
| `ttl(key, durationMs)` | `(string \| Uint8Array, number) => Promise<void>` | Set expiry |
| `scan(prefix)` | `(string \| Uint8Array) => Promise<Array<[Uint8Array, Uint8Array]>>` | Fetch all pairs by prefix |

## Error handling

Communication failures reject with `TricError` (subclass of `Error`). Catch with `try/catch`:

```js
try {
  await conn.write(key, value);
} catch (err) {
  if (err instanceof TricError) {
    console.error('write failed:', err.message);
  } else {
    throw err;
  }
}
```

Absent values → `null`. `cad` mismatch → `false`. The bridge does not retry; the caller decides whether to reconnect.

## Test

Tests live at `bridges/javascript/tests/bridge.test.ts` and use Node's built-in `node:test` runner. No additional test framework installed. Start a scratch server, compile, run:

```bash
cargo build --release

mkdir -p /tmp/tric-js-test
TRIC_SOCKET_DIR=/tmp/tric-js-test \
TRIC_BASE_DIR=/tmp/tric-js-test/data \
TRIC_INSTANCE=jstest TRIC_SLOT=0 \
./target/release/tric server &
SERVER_PID=$!
sleep 2

cd bridges/javascript
npm install
TRIC_SOCKET=/tmp/tric-js-test/server.sock npm test

kill $SERVER_PID
rm -rf /tmp/tric-js-test
```

The test suite exercises all six primitives plus a varied-bytes round-trip: 14 `node:test` cases.

## Next

- [Client Overview](../00-overview.md): the wire protocol from the client perspective, plus the minimum API surface every bridge must provide.
- [Wire Protocol](../../server/04-wire-protocol.md): the full opcode reference, including request and response formats for every primitive.
