// Copyright 2025 Vivian Voss. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0
// Scope: Integration tests for the `write_ttl` primitive on the public `Tric` API.

use std::time::Duration;
use tric::{create_tric, Bytes};

#[test]
fn check_write_ttl_on_existing_key_keeps_value_readable() {
    let tric = create_tric();
    tric.write_value(b"key", b"value");
    tric.write_ttl(b"key", Duration::from_secs(60));
    assert_eq!(tric.read_value(b"key"), Some(Bytes::from_static(b"value")));
}

#[test]
fn check_write_ttl_on_missing_key_does_not_panic() {
    let tric = create_tric();
    tric.write_ttl(b"absent", Duration::from_secs(60));
    assert_eq!(tric.read_value(b"absent"), None);
}

#[test]
fn check_write_ttl_missing_then_write_value_produces_clean_state() {
    let tric = create_tric();
    tric.write_ttl(b"fresh", Duration::from_secs(60));
    tric.write_value(b"fresh", b"finally");
    assert_eq!(
        tric.read_value(b"fresh"),
        Some(Bytes::from_static(b"finally"))
    );
}
