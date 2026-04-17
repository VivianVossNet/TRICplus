// Copyright 2025 Vivian Voss. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0
// Scope: Entry point for tric-server — creates QNX Core, registers modules, starts supervision.

mod core;
mod data_bus;
mod module;

use std::sync::Arc;
use std::time::Duration;

use crate::core::create_core;
use crate::data_bus::{create_tric_bus, DataBus};
use crate::module::{Module, ModuleContext};

struct PlaceholderModule;

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

fn main() {
    let data_bus: Arc<dyn DataBus> = Arc::new(create_tric_bus());
    let mut core = create_core(data_bus);
    core.register_module(|| Box::new(PlaceholderModule));
    core.run_supervision_loop();
}
