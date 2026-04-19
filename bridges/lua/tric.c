/* Copyright 2025-2026 Vivian Voss. Licensed under the BSD 3-Clause License. */
/* SPDX-License-Identifier: BSD-3-Clause */
/* Scope: TRIC+ Lua client — C module exposing tric.connect and Connection metatable. */

#include "tric.h"
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define TRIC_MT "tric.Connection"

typedef struct {
    TricConnection handle;
    int owned;
} LuaTricConnection;

static LuaTricConnection *check_conn(lua_State *L, int index) {
    return (LuaTricConnection *)luaL_checkudata(L, index, TRIC_MT);
}

static int tric_connect(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    LuaTricConnection *lc = (LuaTricConnection *)lua_newuserdata(L, sizeof(LuaTricConnection));
    lc->handle = create_connection(path);
    lc->owned = lc->handle.socket_fd >= 0;
    luaL_getmetatable(L, TRIC_MT);
    lua_setmetatable(L, -2);
    return 1;
}

static int conn_gc(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    if (lc->owned) {
        delete_connection(&lc->handle);
        lc->owned = 0;
    }
    return 0;
}

static int conn_valid(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    lua_pushboolean(L, lc->owned && check_connection(&lc->handle) != 0);
    return 1;
}

static int conn_read(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    size_t key_len;
    const char *key = luaL_checklstring(L, 2, &key_len);
    TricValue v = read_value(&lc->handle, (const uint8_t *)key, key_len);
    if (v.data == NULL) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushlstring(L, (const char *)v.data, v.length);
    delete_value_result(&v);
    return 1;
}

static int conn_write(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    size_t key_len, value_len;
    const char *key = luaL_checklstring(L, 2, &key_len);
    const char *value = luaL_checklstring(L, 3, &value_len);
    int result = write_value(&lc->handle, (const uint8_t *)key, key_len, (const uint8_t *)value, value_len);
    if (result != 0) return luaL_error(L, "write failed");
    return 0;
}

static int conn_del(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    size_t key_len;
    const char *key = luaL_checklstring(L, 2, &key_len);
    int result = delete_value(&lc->handle, (const uint8_t *)key, key_len);
    if (result != 0) return luaL_error(L, "del failed");
    return 0;
}

static int conn_cad(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    size_t key_len, expected_len;
    const char *key = luaL_checklstring(L, 2, &key_len);
    const char *expected = luaL_checklstring(L, 3, &expected_len);
    int result =
        delete_value_if_match(&lc->handle, (const uint8_t *)key, key_len, (const uint8_t *)expected, expected_len);
    if (result < 0) return luaL_error(L, "cad failed");
    lua_pushboolean(L, result == 1);
    return 1;
}

static int conn_ttl(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    size_t key_len;
    const char *key = luaL_checklstring(L, 2, &key_len);
    lua_Integer duration_ms = luaL_checkinteger(L, 3);
    int result = write_ttl(&lc->handle, (const uint8_t *)key, key_len, (uint64_t)duration_ms);
    if (result != 0) return luaL_error(L, "ttl failed");
    return 0;
}

static int conn_scan(lua_State *L) {
    LuaTricConnection *lc = check_conn(L, 1);
    size_t prefix_len;
    const char *prefix = luaL_checklstring(L, 2, &prefix_len);
    TricScanResult sr = find_by_prefix(&lc->handle, (const uint8_t *)prefix, prefix_len);
    lua_createtable(L, (int)sr.count, 0);
    for (size_t i = 0; i < sr.count; ++i) {
        lua_createtable(L, 0, 2);
        lua_pushlstring(L, (const char *)sr.pairs[i].key, sr.pairs[i].key_length);
        lua_setfield(L, -2, "key");
        lua_pushlstring(L, (const char *)sr.pairs[i].value, sr.pairs[i].value_length);
        lua_setfield(L, -2, "value");
        lua_rawseti(L, -2, (lua_Integer)(i + 1));
    }
    delete_scan_result(&sr);
    return 1;
}

static const luaL_Reg conn_methods[] = {{"valid", conn_valid},
                                        {"read", conn_read},
                                        {"write", conn_write},
                                        {"del", conn_del},
                                        {"cad", conn_cad},
                                        {"ttl", conn_ttl},
                                        {"scan", conn_scan},
                                        {NULL, NULL}};

static const luaL_Reg tric_module[] = {{"connect", tric_connect}, {NULL, NULL}};

int luaopen_tric(lua_State *L) {
    luaL_newmetatable(L, TRIC_MT);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, conn_gc);
    lua_setfield(L, -2, "__gc");
    luaL_setfuncs(L, conn_methods, 0);
    lua_pop(L, 1);

    luaL_newlib(L, tric_module);
    return 1;
}
