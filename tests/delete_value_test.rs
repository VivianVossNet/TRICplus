// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: Integration tests for the `delete_value` primitive on the public `Tric` API.

use tric::{create_tric, Bytes};

#[test]
fn check_delete_removes_existing_key() {
    let tric = create_tric();
    tric.write_value(b"key", b"value");
    tric.delete_value(b"key");
    assert_eq!(tric.read_value(b"key"), None);
}

#[test]
fn check_delete_missing_key_is_silent_no_op() {
    let tric = create_tric();
    tric.delete_value(b"absent");
    assert_eq!(tric.read_value(b"absent"), None);
}

#[test]
fn check_delete_does_not_affect_other_keys() {
    let tric = create_tric();
    tric.write_value(b"a", b"1");
    tric.write_value(b"b", b"2");
    tric.delete_value(b"a");
    assert_eq!(tric.read_value(b"a"), None);
    assert_eq!(tric.read_value(b"b"), Some(Bytes::from_static(b"2")));
}
