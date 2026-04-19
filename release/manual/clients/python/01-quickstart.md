# Python Bridge: Quickstart

The TRIC+ Python client speaks the TRIC+ wire protocol directly over a Unix-domain datagram socket. Pure Python 3.10+ against the `socket` standard-library module, no C extension, no compiled binary to build, no runtime dependencies beyond the interpreter. Permutive routing stays on the server: write with a `duration_ms` greater than zero and the value lives in the transient `BTreeMap`; write it with `duration_ms = 0` (the default) and it lives in SQLite. The Python code sees one `Connection` class with six primitives.

## Requirements

- **Python 3.10+** (`python3 --version`)
- A running TRIC+ server reachable via a Unix-domain socket

## Integration

The package is published locally at `bridges/python/`. The recommended path is to install it into a virtual environment with `pip install -e bridges/python` (editable install), or to add `bridges/python/src` to `PYTHONPATH`. Example with pip:

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -e /path/to/TRIC/bridges/python
```

Then use as any other package:

```python
from tric import Connection, TricError
```

## Connect

```python
from tric import Connection

with Connection("/var/run/tric/server.sock") as conn:
    if not conn.valid():
        raise SystemExit("connect failed")
    # ... use the connection ...
```

The `with` block calls `close()` automatically on scope exit. The constructor binds a temporary socket at `/tmp/tric-py-{pid}.sock` and connects to the server; `close()` removes both the file descriptor and the temporary socket.

## Primitives

### Write and read

```python
conn.write("user:42", "alice")

value = conn.read("user:42")
if value is not None:
    print(value.decode())  # "alice"
```

`read` returns `bytes | None`. `None` means the key does not exist, or the read failed; the bridge does not distinguish the two cases.

Both `write` and `read` accept `bytes` or `str`. Strings are UTF-8 encoded automatically; the return type is always `bytes`. For binary data, pass `bytes` directly.

### Delete

```python
conn.delete("user:42")
```

Raises `TricError` on socket failure. Deleting a missing key succeeds silently. The method is named `delete` rather than `del` because `del` is a Python keyword.

### Compare-and-delete

```python
matched = conn.cad("job:1", "pending")
# matched == True:  value was "pending", key is now deleted
# matched == False: value was something else, key is untouched
```

Atomic. Returns `bool`.

### TTL

```python
conn.write("session:abc", "token")
conn.ttl("session:abc", 3_600_000)
```

Duration in milliseconds. Missing key is a silent no-op. Raises `TricError` on socket failure.

### Prefix scan

```python
pairs = conn.scan("user:")
for key, value in pairs:
    print(key, "=", value)
```

Returns `list[tuple[bytes, bytes]]`. Both elements of each tuple are `bytes`; decode to `str` with `.decode()` if the content is UTF-8.

## API

| Method | Signature | Purpose |
|--------|-----------|---------|
| `Connection(socket_path)` | `Connection(str)` | Open a UDS DGRAM connection |
| `close()` | `() -> None` | Close the socket and remove the temporary client path |
| `valid()` | `() -> bool` | Check whether the connection is open |
| `read(key)` | `(bytes \| str) -> bytes \| None` | Fetch a value; `None` if absent |
| `write(key, value, duration_ms=0)` | `(bytes \| str, bytes \| str, int) -> None` | Store a value; `duration_ms > 0` routes to the transient tier |
| `delete(key)` | `(bytes \| str) -> None` | Remove a key |
| `cad(key, expected)` | `(bytes \| str, bytes \| str) -> bool` | Atomic compare-and-delete |
| `ttl(key, duration_ms)` | `(bytes \| str, int) -> None` | Set expiry on an existing key |
| `scan(prefix)` | `(bytes \| str) -> list[tuple[bytes, bytes]]` | Fetch all pairs by prefix |

## Error handling

Communication failures raise `TricError`. Catch with:

```python
try:
    conn.write(key, value)
except TricError as err:
    print(f"write failed: {err}")
```

Absent values → `None`. `cad` mismatch → `False`. The bridge does not retry; the caller decides whether to reconnect.

## Test

Tests live at `bridges/python/tests/test_bridge.py` and use the `unittest` standard-library module, so no `pytest` or other test-framework install is required. Start a scratch server, run the tests, tear down:

```bash
cargo build --release

mkdir -p /tmp/tric-python-test
TRIC_SOCKET_DIR=/tmp/tric-python-test \
TRIC_BASE_DIR=/tmp/tric-python-test/data \
TRIC_INSTANCE=pytest TRIC_SLOT=0 \
./target/release/tric server &
SERVER_PID=$!
sleep 2

cd bridges/python
TRIC_SOCKET=/tmp/tric-python-test/server.sock \
    python3 -m unittest discover tests -v

kill $SERVER_PID
rm -rf /tmp/tric-python-test
```

The test suite exercises all six primitives plus a varied-bytes round-trip: 14 test methods via `unittest`.

## Next

- [Client Overview](../00-overview.md): the wire protocol from the client perspective, plus the minimum API surface every bridge must provide.
- [Wire Protocol](../../server/04-wire-protocol.md): the full opcode reference, including request and response formats for every primitive.
