/* Copyright 2025-2026 Vivian Voss. Licensed under the BSD 3-Clause License. */
/* SPDX-License-Identifier: BSD-3-Clause */
/* Scope: TRIC+ C client — public API for connecting to a TRIC+ server via UDS DGRAM. */

#ifndef TRIC_H
#define TRIC_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int socket_fd;
    uint32_t request_counter;
} TricConnection;

typedef struct {
    const uint8_t *data;
    size_t length;
} TricValue;

typedef struct {
    uint8_t *key;
    size_t key_length;
    uint8_t *value;
    size_t value_length;
} TricPair;

typedef struct {
    TricPair *pairs;
    size_t count;
} TricScanResult;

TricConnection create_connection(const char *socket_path);
void           delete_connection(TricConnection *connection);
int            check_connection(const TricConnection *connection);

TricValue      read_value(TricConnection *connection, const uint8_t *key, size_t key_length);
int            write_value(TricConnection *connection, const uint8_t *key, size_t key_length, const uint8_t *value, size_t value_length);
int            delete_value(TricConnection *connection, const uint8_t *key, size_t key_length);
int            delete_value_if_match(TricConnection *connection, const uint8_t *key, size_t key_length, const uint8_t *expected, size_t expected_length);
int            write_ttl(TricConnection *connection, const uint8_t *key, size_t key_length, uint64_t duration_ms);
TricScanResult find_by_prefix(TricConnection *connection, const uint8_t *prefix, size_t prefix_length);

void           delete_value_result(TricValue *value);
void           delete_scan_result(TricScanResult *result);

#endif
