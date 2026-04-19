-- SPDX-License-Identifier: BSD-3-Clause
-- Copyright (c) 2025-2026 Vivian Voss
-- Scope: Integration test for the TRIC+ Lua bridge — verifies all six primitives against a running server.

local script_dir = arg[0]:match("(.*/)") or "./"
package.cpath = script_dir .. "../?.dylib;" .. script_dir .. "../?.so;" .. package.cpath

local tric = require("tric")

local socketPath = os.getenv("TRIC_SOCKET") or "/tmp/tric-lua-test/server.sock"
local conn = tric.connect(socketPath)

local passed = 0
local failed = 0

local function check(label, condition)
    if condition then
        passed = passed + 1
    else
        failed = failed + 1
        io.stderr:write("FAIL: " .. label .. "\n")
    end
end

check("connection is valid", conn:valid())

conn:write("test:1", "hello")
check("read returns value", conn:read("test:1") ~= nil)

conn:write("test:len", "hello")
check("read returns correct length", #conn:read("test:len") == 5)

conn:write("test:content", "hello")
check("read returns correct content", conn:read("test:content") == "hello")

conn:write("test:over", "original")
conn:write("test:over", "updated")
check("write overwrites", conn:read("test:over") == "updated")

conn:write("test:del", "payload")
conn:del("test:del")
check("del removes key", conn:read("test:del") == nil)

conn:write("test:cas-miss", "original")
check("cad mismatch returns false", not conn:cad("test:cas-miss", "wrong"))

conn:write("test:cas-keep", "original")
conn:cad("test:cas-keep", "wrong")
check("cad mismatch keeps value", conn:read("test:cas-keep") ~= nil)

conn:write("test:cas-match", "original")
check("cad match returns true", conn:cad("test:cas-match", "original"))

conn:write("test:cas-del", "original")
conn:cad("test:cas-del", "original")
check("cad match deletes", conn:read("test:cas-del") == nil)

conn:write("test:ttl", "ephemeral")
local ok = pcall(function() conn:ttl("test:ttl", 60000) end)
check("ttl succeeds", ok)

conn:write("test:ttl-read", "ephemeral")
conn:ttl("test:ttl-read", 60000)
check("ttl key still readable", conn:read("test:ttl-read") ~= nil)

conn:write("scan:a", "1")
conn:write("scan:b", "2")
conn:write("scan:c", "3")
local pairs_result = conn:scan("scan:")
check("scan returns results", #pairs_result >= 3)
conn:del("scan:a")
conn:del("scan:b")
conn:del("scan:c")

conn:write("test:slice", "value with spaces and more bytes")
check("round-trip varied bytes", conn:read("test:slice") == "value with spaces and more bytes")

print(string.format("%d passed, %d failed", passed, failed))
if failed > 0 then os.exit(1) end
