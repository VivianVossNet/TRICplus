// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: Integration tests for the `delete_value_if_match` primitive on the public `Tric` API.

use tric::{create_tric, Bytes};

#[test]
fn check_match_deletes_entry_and_returns_true() {
    let tric = create_tric();
    tric.write_value(b"key", b"expected");
    assert!(tric.delete_value_if_match(b"key", b"expected"));
    assert_eq!(tric.read_value(b"key"), None);
}

#[test]
fn check_mismatch_keeps_entry_and_returns_false() {
    let tric = create_tric();
    tric.write_value(b"key", b"actual");
    assert!(!tric.delete_value_if_match(b"key", b"other"));
    assert_eq!(tric.read_value(b"key"), Some(Bytes::from_static(b"actual")));
}

#[test]
fn check_missing_key_returns_false() {
    let tric = create_tric();
    assert!(!tric.delete_value_if_match(b"absent", b"anything"));
    assert_eq!(tric.read_value(b"absent"), None);
}
