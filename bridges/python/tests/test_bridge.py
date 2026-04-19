# Copyright (c) 2025-2026 Vivian Voss
# SPDX-License-Identifier: BSD-3-Clause
# Scope: Integration test for the TRIC+ Python bridge.
# Verifies all six primitives against a running server.

from __future__ import annotations

import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

from tric import Connection

DEFAULT_SOCKET = "/tmp/tric-python-test/server.sock"


def _socket_path() -> str:
    return os.environ.get("TRIC_SOCKET", DEFAULT_SOCKET)


class TricBridgeTest(unittest.TestCase):
    def setUp(self) -> None:
        self.conn = Connection(_socket_path())

    def tearDown(self) -> None:
        self.conn.close()

    def test_connection_is_valid(self) -> None:
        self.assertTrue(self.conn.valid())

    def test_read_returns_written_value(self) -> None:
        self.conn.write("test:1", "hello")
        self.assertIsNotNone(self.conn.read("test:1"))

    def test_read_returns_correct_length(self) -> None:
        self.conn.write("test:len", "hello")
        value = self.conn.read("test:len")
        assert value is not None
        self.assertEqual(len(value), 5)

    def test_read_returns_correct_content(self) -> None:
        self.conn.write("test:content", "hello")
        self.assertEqual(self.conn.read("test:content"), b"hello")

    def test_write_overwrites(self) -> None:
        self.conn.write("test:over", "original")
        self.conn.write("test:over", "updated")
        self.assertEqual(self.conn.read("test:over"), b"updated")

    def test_delete_removes_key(self) -> None:
        self.conn.write("test:del", "payload")
        self.conn.delete("test:del")
        self.assertIsNone(self.conn.read("test:del"))

    def test_cad_mismatch_returns_false(self) -> None:
        self.conn.write("test:cas-miss", "original")
        self.assertFalse(self.conn.cad("test:cas-miss", "wrong"))

    def test_cad_mismatch_keeps_value(self) -> None:
        self.conn.write("test:cas-keep", "original")
        self.conn.cad("test:cas-keep", "wrong")
        self.assertIsNotNone(self.conn.read("test:cas-keep"))

    def test_cad_match_returns_true(self) -> None:
        self.conn.write("test:cas-match", "original")
        self.assertTrue(self.conn.cad("test:cas-match", "original"))

    def test_cad_match_deletes(self) -> None:
        self.conn.write("test:cas-del", "original")
        self.conn.cad("test:cas-del", "original")
        self.assertIsNone(self.conn.read("test:cas-del"))

    def test_ttl_succeeds(self) -> None:
        self.conn.write("test:ttl", "ephemeral")
        self.conn.ttl("test:ttl", 60_000)

    def test_ttl_key_still_readable(self) -> None:
        self.conn.write("test:ttl-read", "ephemeral")
        self.conn.ttl("test:ttl-read", 60_000)
        self.assertIsNotNone(self.conn.read("test:ttl-read"))

    def test_scan_returns_results(self) -> None:
        self.conn.write("scan:a", "1")
        self.conn.write("scan:b", "2")
        self.conn.write("scan:c", "3")
        pairs = self.conn.scan("scan:")
        self.assertGreaterEqual(len(pairs), 3)
        self.conn.delete("scan:a")
        self.conn.delete("scan:b")
        self.conn.delete("scan:c")

    def test_roundtrip_varied_bytes(self) -> None:
        value = b"value with spaces and more bytes"
        self.conn.write(b"test:slice", value)
        self.assertEqual(self.conn.read(b"test:slice"), value)


if __name__ == "__main__":
    unittest.main()
