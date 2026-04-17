// Copyright 2025 Vivian Voss. Licensed under the Apache License, Version 2.0.
// SPDX-License-Identifier: Apache-2.0
// Scope: Module trait and ModuleContext — interface for QNX-supervised server modules.

use std::sync::Arc;

use crate::Tric;

use super::data_bus::DataBus;

pub struct ModuleContext {
    pub core_bus: Tric,
    pub data_bus: Arc<dyn DataBus>,
}

pub trait Module: Send + 'static {
    fn name(&self) -> &'static str;
    fn run(&self, context: ModuleContext);
}
