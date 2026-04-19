#!/bin/sh
# TRIC+ Benchmark Runner — executed as root on primus
# Rolls back ZFS snapshot, starts jail, waits, runs benchmark.

set -e

JAIL="tric"
DATASET="rpool/jails/tric"
SNAPSHOT="${DATASET}@bench-clean"
TIMESTAMP=$(date -u +%Y-%m-%dT%H:%M:%SZ)
PLATFORM="freebsd-15-ryzen3600-zfs"

echo "=== TRIC+ Benchmark Run ==="
echo "Timestamp: $TIMESTAMP"
echo "Platform:  $PLATFORM"
echo ""

# 1. Stop jail
echo "[1/5] Stopping jail..."
service jail stop "$JAIL" 2>/dev/null || true
sleep 2

# 2. Rollback ZFS snapshot
echo "[2/5] Rolling back to clean snapshot..."
zfs rollback -r "$SNAPSHOT"

# 3. Start jail
echo "[3/5] Starting jail..."
service jail start "$JAIL"
sleep 2

# 4. Start Redis + pull latest + rebuild
echo "[4/5] Starting Redis, pulling, building..."
jexec "$JAIL" sh -c 'redis-server --daemonize yes --bind 10.0.0.70 --requirepass tric-bench'
jexec "$JAIL" sh -c 'cd /root/TRIC && git pull && cargo build --release 2>&1' | tail -3
COMMIT=$(jexec "$JAIL" sh -c 'cd /root/TRIC && git rev-parse --short HEAD')
echo "Commit:    $COMMIT"

# 5. Thermal stabilisation + benchmark
echo "[5/5] Thermal idle (30s) then benchmark..."
sleep 30
jexec "$JAIL" sh -c 'cd /root/TRIC && REDIS_URL="redis://:tric-bench@10.0.0.70/" cargo test --release --test benchmark_test -- --ignored --nocapture' 2>&1 | tee /tmp/bench-output.txt

echo ""
echo "=== Done ==="
