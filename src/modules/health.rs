// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: Health — PING (0x13) handling, bidirectional liveness probe.

use crate::modules::codec::{Request, Response};

const OPCODE_PING: u8 = 0x13;
const OPCODE_OK: u8 = 0x80;

pub fn check_ping(request: &Request) -> Option<Response> {
    if request.opcode != OPCODE_PING {
        return None;
    }
    Some(Response {
        request_id: request.request_id,
        opcode: OPCODE_OK,
        payload: Vec::new(),
    })
}
