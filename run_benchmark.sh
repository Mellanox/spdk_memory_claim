#!/bin/bash

set -e

# Build the benchmark application
make

# --- Test Case 1: Without persistent Shared Memory ---
echo "****************************************************************"
echo "--- Running Benchmark: WITHOUT Persistent Memory (Anonymous Hugepages) ---"
echo "--- Expected result: Reclaim time should be slow. Hugepages will be freed on kill. ---"
echo "****************************************************************"
echo

echo "---"
echo "--- Initial Hugepage Status ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo
echo "--- Running benchmark: Allocation and Free Time ---"
./memory_benchmark alloc_free

echo
echo "---"
echo "--- Hugepage Status After alloc/free ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo
echo "--- Running benchmark: Reclaim Time (Anonymous Pages) ---"
echo "Step 1: Starting the first process to allocate memory..."
./memory_benchmark alloc_wait &
BENCH_PID=$!
sleep 5

echo
echo "--- Hugepage Status while process is running ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo "Step 2: Killing the first process... (Kernel should reclaim pages)"
kill -9 $BENCH_PID
wait $BENCH_PID || true

echo
echo "--- Hugepage Status after process is killed ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo "Step 3: Starting the second process to 'reclaim' memory..."
./memory_benchmark reclaim

echo
echo "--- Hugepage Status after 'reclaim' and free ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"


# --- Test Case 2: With persistent Shared Memory ---
echo
echo "****************************************************************"
echo "--- Running Benchmark: WITH Persistent Memory (File-Backed Hugepages) ---"
echo "--- Expected result: Reclaim time should be fast. Hugepages will persist on kill. ---"
echo "****************************************************************"
echo

echo "---"
echo "--- Initial Hugepage Status ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo
echo "--- Running benchmark: Reclaim Time (File-Backed Pages) ---"
echo "Step 1: Starting the first process to allocate memory..."
./memory_benchmark alloc_wait use_shm &
BENCH_PID=$!
sleep 5

echo
echo "--- Hugepage Status while process is running ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo "Step 2: Killing the first process... (Pages should persist)"
kill -9 $BENCH_PID
wait $BENCH_PID || true

echo
echo "--- Hugepage Status after process is killed ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo "Step 3: Starting the second process to reclaim memory..."
./memory_benchmark reclaim use_shm

echo
echo "--- Hugepage Status after reclaim and free ---"
grep -E 'HugePages_Total|HugePages_Free' /proc/meminfo
echo "---"

echo
echo "--- Benchmark complete ---" 