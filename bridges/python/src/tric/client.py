# Copyright (c) 2025-2026 Vivian Voss
# SPDX-License-Identifier: BSD-3-Clause
# Scope: TRIC+ Python client — Connection class speaking the TRIC+ wire protocol over UDS DGRAM.

from __future__ import annotations

import os
import socket
import struct
from pathlib import Path
from types import TracebackType

OP_READ = 0x01
OP_WRITE = 0x02
OP_DELETE = 0x03
OP_CAD = 0x04
OP_TTL = 0x05
OP_SCAN = 0x06

RESP_OK = 0x80
RESP_OK_DATA = 0x81
RESP_SCAN_CHUNK = 0x90
RESP_SCAN_END = 0x91

MAX_DATAGRAM = 65536


class TricError(Exception):
    """Raised on TRIC+ wire-protocol or socket failure."""


def _to_bytes(value: bytes | str) -> bytes:
    return value.encode("utf-8") if isinstance(value, str) else value


class Connection:
    """Synchronous TRIC+ client over a Unix-domain datagram socket.

    Opens a temporary bound client socket at `/tmp/tric-py-{pid}.sock` and
    connects to the TRIC+ server at the given socket path. Use as a context
    manager for automatic cleanup, or call `close()` explicitly.
    """

    def __init__(self, socket_path: str) -> None:
        self._request_id: int = 1
        self._client_path = Path(f"/tmp/tric-py-{os.getpid()}.sock")
        self._server_path = socket_path
        self._owned = False
        self._socket = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)

        if self._client_path.exists():
            self._client_path.unlink()

        try:
            self._socket.bind(str(self._client_path))
            self._socket.connect(socket_path)
            self._owned = True
        except OSError as err:
            self._socket.close()
            raise TricError(f"connect failed: {err}") from err

    def __enter__(self) -> Connection:
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: TracebackType | None,
    ) -> None:
        self.close()

    def close(self) -> None:
        if self._owned:
            self._socket.close()
            self._owned = False
            if self._client_path.exists():
                self._client_path.unlink()

    def valid(self) -> bool:
        return self._owned

    def _next_request_id(self) -> int:
        rid = self._request_id
        self._request_id = (self._request_id + 1) & 0xFFFFFFFF
        return rid

    def _roundtrip(self, opcode: int, payload: bytes) -> tuple[int, bytes]:
        rid = self._next_request_id()
        header = struct.pack(">IB", rid, opcode)
        self._socket.send(header + payload)
        reply = self._socket.recv(MAX_DATAGRAM)
        if len(reply) < 5:
            raise TricError("malformed response")
        reply_rid, reply_op = struct.unpack(">IB", reply[:5])
        if reply_rid != rid:
            raise TricError(f"request-id mismatch: sent {rid}, got {reply_rid}")
        return reply_op, reply[5:]

    def read(self, key: bytes | str) -> bytes | None:
        key_b = _to_bytes(key)
        payload = struct.pack(">I", len(key_b)) + key_b
        op, body = self._roundtrip(OP_READ, payload)
        if op == RESP_OK_DATA:
            if len(body) < 4:
                raise TricError("malformed OK_DATA response")
            (value_len,) = struct.unpack(">I", body[:4])
            return body[4 : 4 + value_len]
        if op == RESP_OK:
            return None
        return None

    def write(self, key: bytes | str, value: bytes | str, duration_ms: int = 0) -> None:
        key_b = _to_bytes(key)
        value_b = _to_bytes(value)
        payload = (
            struct.pack(">I", len(key_b))
            + key_b
            + struct.pack(">I", len(value_b))
            + value_b
            + struct.pack(">Q", duration_ms)
        )
        op, _ = self._roundtrip(OP_WRITE, payload)
        if op != RESP_OK:
            raise TricError(f"write failed, opcode 0x{op:02x}")

    def delete(self, key: bytes | str) -> None:
        key_b = _to_bytes(key)
        payload = struct.pack(">I", len(key_b)) + key_b
        op, _ = self._roundtrip(OP_DELETE, payload)
        if op != RESP_OK:
            raise TricError(f"delete failed, opcode 0x{op:02x}")

    def cad(self, key: bytes | str, expected: bytes | str) -> bool:
        key_b = _to_bytes(key)
        expected_b = _to_bytes(expected)
        payload = (
            struct.pack(">I", len(key_b)) + key_b + struct.pack(">I", len(expected_b)) + expected_b
        )
        op, body = self._roundtrip(OP_CAD, payload)
        return op == RESP_OK_DATA and len(body) >= 1 and body[0] == 0x01

    def ttl(self, key: bytes | str, duration_ms: int) -> None:
        key_b = _to_bytes(key)
        payload = struct.pack(">I", len(key_b)) + key_b + struct.pack(">Q", duration_ms)
        op, _ = self._roundtrip(OP_TTL, payload)
        if op != RESP_OK:
            raise TricError(f"ttl failed, opcode 0x{op:02x}")

    def scan(self, prefix: bytes | str) -> list[tuple[bytes, bytes]]:
        prefix_b = _to_bytes(prefix)
        payload = struct.pack(">I", len(prefix_b)) + prefix_b
        rid = self._next_request_id()
        header = struct.pack(">IB", rid, OP_SCAN)
        self._socket.send(header + payload)
        pairs: list[tuple[bytes, bytes]] = []
        while True:
            reply = self._socket.recv(MAX_DATAGRAM)
            if len(reply) < 5:
                raise TricError("malformed scan reply")
            reply_rid, reply_op = struct.unpack(">IB", reply[:5])
            if reply_rid != rid:
                raise TricError(f"scan request-id mismatch: sent {rid}, got {reply_rid}")
            if reply_op == RESP_SCAN_END:
                return pairs
            if reply_op != RESP_SCAN_CHUNK:
                raise TricError(f"scan unexpected opcode 0x{reply_op:02x}")
            body = reply[5:]
            if len(body) < 4:
                raise TricError("malformed scan chunk header")
            offset = 4
            (key_len,) = struct.unpack(">I", body[offset : offset + 4])
            offset += 4
            key = body[offset : offset + key_len]
            offset += key_len
            (value_len,) = struct.unpack(">I", body[offset : offset + 4])
            offset += 4
            value = body[offset : offset + value_len]
            pairs.append((key, value))
