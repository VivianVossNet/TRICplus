// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: Integration tests for concurrent access to `Tric` via cloned handles across threads.

use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::Arc;
use std::thread;
use tric::{create_tric, Bytes};

#[test]
fn check_parallel_writes_to_distinct_keys_all_visible() {
    let tric = create_tric();
    let threads = 10;
    let per_thread = 100;

    let handles: Vec<_> = (0..threads)
        .map(|thread_index| {
            let tric_handle = tric.clone();
            thread::spawn(move || {
                for item_index in 0..per_thread {
                    let key = format!("t{thread_index}:k{item_index}");
                    let value = format!("v{thread_index}_{item_index}");
                    tric_handle.write_value(key.as_bytes(), value.as_bytes());
                }
            })
        })
        .collect();

    for handle in handles {
        handle.join().unwrap();
    }

    for thread_index in 0..threads {
        for item_index in 0..per_thread {
            let key = format!("t{thread_index}:k{item_index}");
            let expected = format!("v{thread_index}_{item_index}");
            assert_eq!(
                tric.read_value(key.as_bytes()),
                Some(Bytes::copy_from_slice(expected.as_bytes()))
            );
        }
    }
}

#[test]
fn check_concurrent_cas_yields_exactly_one_winner() {
    let tric = create_tric();
    tric.write_value(b"lock", b"free");

    let winners = Arc::new(AtomicUsize::new(0));
    let threads = 20;

    let handles: Vec<_> = (0..threads)
        .map(|_| {
            let tric_handle = tric.clone();
            let winners_handle = Arc::clone(&winners);
            thread::spawn(move || {
                if tric_handle.delete_value_if_match(b"lock", b"free") {
                    winners_handle.fetch_add(1, Ordering::SeqCst);
                }
            })
        })
        .collect();

    for handle in handles {
        handle.join().unwrap();
    }

    assert_eq!(winners.load(Ordering::SeqCst), 1);
    assert_eq!(tric.read_value(b"lock"), None);
}

#[test]
fn check_cloned_handles_share_the_same_store() {
    let tric = create_tric();
    let writer_handle = tric.clone();
    let reader_handle = tric.clone();

    thread::spawn(move || {
        writer_handle.write_value(b"shared", b"yes");
    })
    .join()
    .unwrap();

    assert_eq!(
        reader_handle.read_value(b"shared"),
        Some(Bytes::from_static(b"yes"))
    );
    assert_eq!(tric.read_value(b"shared"), Some(Bytes::from_static(b"yes")));
}

#[test]
fn check_scan_during_concurrent_writes_returns_valid_snapshots() {
    let tric = create_tric();
    let writes = 200;
    let scans = 50;

    let writer = {
        let tric_handle = tric.clone();
        thread::spawn(move || {
            for index in 0..writes {
                let key = format!("item:{index:03}");
                tric_handle.write_value(key.as_bytes(), b"v");
            }
        })
    };

    let scanner = {
        let tric_handle = tric.clone();
        thread::spawn(move || {
            for _ in 0..scans {
                let result = tric_handle.find_by_prefix(b"item:");
                for (key, value) in &result {
                    assert!(key.starts_with(b"item:"));
                    assert_eq!(value.as_ref(), b"v");
                }
            }
        })
    };

    writer.join().unwrap();
    scanner.join().unwrap();

    let final_result = tric.find_by_prefix(b"item:");
    assert_eq!(final_result.len(), writes);
}
