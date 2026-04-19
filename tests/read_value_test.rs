// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: Integration tests for the `read_value` primitive on the public `Tric` API.

use tric::{create_tric, Bytes};

#[test]
fn check_missing_key_returns_none() {
    let tric = create_tric();
    assert_eq!(tric.read_value(b"missing"), None);
}

#[test]
fn check_existing_key_returns_written_value() {
    let tric = create_tric();
    tric.write_value(b"key", b"value");
    assert_eq!(tric.read_value(b"key"), Some(Bytes::from_static(b"value")));
}

#[test]
fn check_repeated_reads_are_consistent() {
    let tric = create_tric();
    tric.write_value(b"key", b"value");
    let first = tric.read_value(b"key");
    let second = tric.read_value(b"key");
    assert_eq!(first, second);
    assert!(first.is_some());
}
