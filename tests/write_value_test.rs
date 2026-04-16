// Copyright 2025 Vivian Voss. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0
// Scope: Integration tests for the `write_value` primitive on the public `Tric` API.

use tric::{create_tric, Bytes};

#[test]
fn check_new_key_becomes_readable() {
    let tric = create_tric();
    tric.write_value(b"fresh", b"data");
    assert_eq!(tric.read_value(b"fresh"), Some(Bytes::from_static(b"data")));
}

#[test]
fn check_overwrite_replaces_value() {
    let tric = create_tric();
    tric.write_value(b"key", b"first");
    tric.write_value(b"key", b"second");
    assert_eq!(tric.read_value(b"key"), Some(Bytes::from_static(b"second")));
}

#[test]
fn check_independent_keys_do_not_interfere() {
    let tric = create_tric();
    tric.write_value(b"a", b"1");
    tric.write_value(b"b", b"2");
    assert_eq!(tric.read_value(b"a"), Some(Bytes::from_static(b"1")));
    assert_eq!(tric.read_value(b"b"), Some(Bytes::from_static(b"2")));
}
