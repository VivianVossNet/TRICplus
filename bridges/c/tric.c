/* Copyright 2025 Vivian Voss. Licensed under the Apache License, Version 2.0. */
/* SPDX-License-Identifier: Apache-2.0 */
/* Scope: TRIC+ C client — UDS DGRAM wire protocol, zero dependencies beyond POSIX. */

#include "tric.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

static void write_u32_be(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value >> 24);
    buffer[1] = (uint8_t)(value >> 16);
    buffer[2] = (uint8_t)(value >> 8);
    buffer[3] = (uint8_t)(value);
}

static void write_u64_be(uint8_t *buffer, uint64_t value) {
    buffer[0] = (uint8_t)(value >> 56);
    buffer[1] = (uint8_t)(value >> 48);
    buffer[2] = (uint8_t)(value >> 40);
    buffer[3] = (uint8_t)(value >> 32);
    buffer[4] = (uint8_t)(value >> 24);
    buffer[5] = (uint8_t)(value >> 16);
    buffer[6] = (uint8_t)(value >> 8);
    buffer[7] = (uint8_t)(value);
}

static uint32_t read_u32_be(const uint8_t *buffer) {
    return ((uint32_t)buffer[0] << 24)
         | ((uint32_t)buffer[1] << 16)
         | ((uint32_t)buffer[2] << 8)
         | ((uint32_t)buffer[3]);
}

static ssize_t write_request(TricConnection *connection, uint8_t opcode,
                             const uint8_t *payload, size_t payload_length,
                             uint8_t *buffer, size_t buffer_size) {
    size_t total = 5 + payload_length;
    if (total > buffer_size) return -1;

    write_u32_be(buffer, connection->request_counter++);
    buffer[4] = opcode;
    if (payload_length > 0) {
        memcpy(buffer + 5, payload, payload_length);
    }

    ssize_t sent = send(connection->socket_fd, buffer, total, 0);
    if (sent < 0) return -1;

    return recv(connection->socket_fd, buffer, buffer_size, 0);
}

TricConnection create_connection(const char *socket_path) {
    TricConnection connection;
    connection.socket_fd = -1;
    connection.request_counter = 1;

    int fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0) return connection;

    struct sockaddr_un client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sun_family = AF_UNIX;
    snprintf(client_addr.sun_path, sizeof(client_addr.sun_path),
             "/tmp/tric-c-%d.sock", getpid());
    unlink(client_addr.sun_path);

    if (bind(fd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        close(fd);
        return connection;
    }

    struct sockaddr_un server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        unlink(client_addr.sun_path);
        close(fd);
        return connection;
    }

    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    connection.socket_fd = fd;
    return connection;
}

void delete_connection(TricConnection *connection) {
    if (connection->socket_fd < 0) return;

    struct sockaddr_un addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(connection->socket_fd, (struct sockaddr *)&addr, &addr_len) == 0) {
        unlink(addr.sun_path);
    }
    close(connection->socket_fd);
    connection->socket_fd = -1;
}

int check_connection(const TricConnection *connection) {
    return connection->socket_fd >= 0;
}

TricValue read_value(TricConnection *connection, const uint8_t *key, size_t key_length) {
    TricValue result = {NULL, 0};
    uint8_t buffer[2048];
    uint8_t payload[4 + 2048];

    write_u32_be(payload, (uint32_t)key_length);
    memcpy(payload + 4, key, key_length);

    ssize_t received = write_request(connection, 0x01, payload, 4 + key_length, buffer, sizeof(buffer));
    if (received < 5) return result;

    if (buffer[4] == 0x81 && received >= 9) {
        uint32_t value_length = read_u32_be(buffer + 5);
        if ((size_t)received >= 9 + value_length) {
            uint8_t *data = (uint8_t *)malloc(value_length);
            if (data) {
                memcpy(data, buffer + 9, value_length);
                result.data = data;
                result.length = value_length;
            }
        }
    }

    return result;
}

int write_value(TricConnection *connection, const uint8_t *key, size_t key_length,
                const uint8_t *value, size_t value_length) {
    uint8_t buffer[2048];
    uint8_t payload[4 + 2048 + 4 + 2048 + 8];
    size_t offset = 0;

    write_u32_be(payload + offset, (uint32_t)key_length);
    offset += 4;
    memcpy(payload + offset, key, key_length);
    offset += key_length;
    write_u32_be(payload + offset, (uint32_t)value_length);
    offset += 4;
    memcpy(payload + offset, value, value_length);
    offset += value_length;
    write_u64_be(payload + offset, 0);
    offset += 8;

    ssize_t received = write_request(connection, 0x02, payload, offset, buffer, sizeof(buffer));
    if (received < 5) return -1;

    return buffer[4] == 0x80 ? 0 : -1;
}

int delete_value(TricConnection *connection, const uint8_t *key, size_t key_length) {
    uint8_t buffer[2048];
    uint8_t payload[4 + 2048];

    write_u32_be(payload, (uint32_t)key_length);
    memcpy(payload + 4, key, key_length);

    ssize_t received = write_request(connection, 0x03, payload, 4 + key_length, buffer, sizeof(buffer));
    if (received < 5) return -1;

    return buffer[4] == 0x80 ? 0 : -1;
}

int delete_value_if_match(TricConnection *connection, const uint8_t *key, size_t key_length,
                          const uint8_t *expected, size_t expected_length) {
    uint8_t buffer[2048];
    uint8_t payload[4 + 2048 + 4 + 2048];
    size_t offset = 0;

    write_u32_be(payload + offset, (uint32_t)key_length);
    offset += 4;
    memcpy(payload + offset, key, key_length);
    offset += key_length;
    write_u32_be(payload + offset, (uint32_t)expected_length);
    offset += 4;
    memcpy(payload + offset, expected, expected_length);
    offset += expected_length;

    ssize_t received = write_request(connection, 0x04, payload, offset, buffer, sizeof(buffer));
    if (received < 6) return -1;

    return (buffer[4] == 0x81 && buffer[5] == 0x01) ? 1 : 0;
}

int write_ttl(TricConnection *connection, const uint8_t *key, size_t key_length, uint64_t duration_ms) {
    uint8_t buffer[2048];
    uint8_t payload[4 + 2048 + 8];
    size_t offset = 0;

    write_u32_be(payload + offset, (uint32_t)key_length);
    offset += 4;
    memcpy(payload + offset, key, key_length);
    offset += key_length;
    write_u64_be(payload + offset, duration_ms);
    offset += 8;

    ssize_t received = write_request(connection, 0x05, payload, offset, buffer, sizeof(buffer));
    if (received < 5) return -1;

    return buffer[4] == 0x80 ? 0 : -1;
}

TricScanResult find_by_prefix(TricConnection *connection, const uint8_t *prefix, size_t prefix_length) {
    TricScanResult result = {NULL, 0};
    uint8_t buffer[65536];
    uint8_t payload[4 + 2048];

    write_u32_be(payload, (uint32_t)prefix_length);
    memcpy(payload + 4, prefix, prefix_length);

    size_t total = 5 + 4 + prefix_length;
    write_u32_be(buffer, connection->request_counter++);
    buffer[4] = 0x06;
    memcpy(buffer + 5, payload, 4 + prefix_length);

    if (send(connection->socket_fd, buffer, total, 0) < 0) return result;

    size_t capacity = 16;
    result.pairs = (TricPair *)malloc(capacity * sizeof(TricPair));
    if (!result.pairs) return result;

    while (1) {
        ssize_t received = recv(connection->socket_fd, buffer, sizeof(buffer), 0);
        if (received < 5) break;

        if (buffer[4] == 0x91) break;

        if (buffer[4] == 0x90 && received > 9) {
            size_t offset = 9;
            if (offset + 4 > (size_t)received) continue;
            uint32_t key_length = read_u32_be(buffer + offset);
            offset += 4;
            if (offset + key_length + 4 > (size_t)received) continue;

            uint8_t *key = (uint8_t *)malloc(key_length);
            if (!key) continue;
            memcpy(key, buffer + offset, key_length);
            offset += key_length;

            uint32_t value_length = read_u32_be(buffer + offset);
            offset += 4;
            if (offset + value_length > (size_t)received) { free(key); continue; }

            uint8_t *value = (uint8_t *)malloc(value_length);
            if (!value) { free(key); continue; }
            memcpy(value, buffer + offset, value_length);

            if (result.count >= capacity) {
                capacity *= 2;
                TricPair *grown = (TricPair *)realloc(result.pairs, capacity * sizeof(TricPair));
                if (!grown) { free(key); free(value); break; }
                result.pairs = grown;
            }

            result.pairs[result.count].key = key;
            result.pairs[result.count].key_length = key_length;
            result.pairs[result.count].value = value;
            result.pairs[result.count].value_length = value_length;
            result.count++;
        }
    }

    return result;
}

void delete_value_result(TricValue *value) {
    if (value->data) {
        free((void *)value->data);
        value->data = NULL;
        value->length = 0;
    }
}

void delete_scan_result(TricScanResult *result) {
    for (size_t i = 0; i < result->count; i++) {
        free(result->pairs[i].key);
        free(result->pairs[i].value);
    }
    free(result->pairs);
    result->pairs = NULL;
    result->count = 0;
}
