// Copyright 2025-2026 Vivian Voss. Licensed under the Business Source License 1.1.
// SPDX-License-Identifier: BUSL-1.1
// Scope: Module trait and ModuleContext — interface for supervised server modules.

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
