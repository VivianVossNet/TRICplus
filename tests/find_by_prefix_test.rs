// Copyright 2025 Vivian Voss. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0
// Scope: Integration tests for the `find_by_prefix` primitive on the public `Tric` API.

use tric::{create_tric, Bytes};

#[test]
fn check_empty_store_returns_empty_vec() {
    let tric = create_tric();
    let result = tric.find_by_prefix(b"anything");
    assert!(result.is_empty());
}

#[test]
fn check_no_match_returns_empty_vec() {
    let tric = create_tric();
    tric.write_value(b"user:1", b"alice");
    let result = tric.find_by_prefix(b"session:");
    assert!(result.is_empty());
}

#[test]
fn check_all_matching_pairs_are_returned_sorted() {
    let tric = create_tric();
    tric.write_value(b"user:1", b"alice");
    tric.write_value(b"user:2", b"bob");
    tric.write_value(b"session:x", b"token");
    let result = tric.find_by_prefix(b"user:");
    assert_eq!(
        result,
        vec![
            (Bytes::from_static(b"user:1"), Bytes::from_static(b"alice"),),
            (Bytes::from_static(b"user:2"), Bytes::from_static(b"bob")),
        ]
    );
}

#[test]
fn check_exact_key_equals_prefix_is_included() {
    let tric = create_tric();
    tric.write_value(b"prefix", b"value");
    tric.write_value(b"prefix_other", b"second");
    let result = tric.find_by_prefix(b"prefix");
    assert_eq!(result.len(), 2);
    assert_eq!(result[0].0, Bytes::from_static(b"prefix"));
    assert_eq!(result[1].0, Bytes::from_static(b"prefix_other"));
}
