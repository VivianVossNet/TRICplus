// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: PlaceholderModule — validates the Core supervision loop via heartbeat.

use std::time::Duration;

use crate::core::module::{Module, ModuleContext};

pub struct PlaceholderModule;

impl Module for PlaceholderModule {
    fn name(&self) -> &'static str {
        "placeholder"
    }

    fn run(&self, context: ModuleContext) {
        let module_key = b"module:placeholder";
        let _ = context.data_bus.read_value(b"probe");
        loop {
            context
                .core_bus
                .write_ttl(module_key, Duration::from_secs(15));
            std::thread::sleep(Duration::from_secs(5));
        }
    }
}
