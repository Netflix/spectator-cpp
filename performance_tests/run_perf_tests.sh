#!/bin/bash

set -e

BINARY="${1:-./performance_test}"
WRITER="${2:-both}"
RESULTS_DIR="results_$(date +%Y%m%d_%H%M%S)"

if [[ "$WRITER" != "udp" && "$WRITER" != "uds" && "$WRITER" != "both" ]]; then
    echo "Usage: $0 [binary] [udp|uds|both]" >&2
    exit 1
fi

declare -A MODE_NAMES=(
    [0]="NonBuffered"
    [1]="Buffered"
    [2]="ThreadLocalBuffered"
)

mkdir -p "$RESULTS_DIR"

run_test() {
    local writer=$1
    local mode=$2
    local threads=$3
    local mode_name="${MODE_NAMES[$mode]}"
    local outfile="${RESULTS_DIR}/${writer}_${mode_name}_${threads}thread.txt"

    echo "Running: writer=${writer} mode=${mode_name} threads=${threads}"
    "$BINARY" "$writer" "$mode" "$threads" | tee "$outfile"
    echo ""
}

writers=()
if [[ "$WRITER" == "both" ]]; then
    writers=(uds udp)
else
    writers=("$WRITER")
fi

for writer in "${writers[@]}"; do
    echo "=== ${writer^^}: Single Thread Baseline (all modes) ==="
    for mode in 0 1 2; do
        run_test "$writer" "$mode" 1
    done

    echo "=== ${writer^^}: Multi-Thread Scaling (buffered modes only) ==="
    for threads in 2 4 8; do
        for mode in 1 2; do
            run_test "$writer" "$mode" "$threads"
        done
    done
done

echo "Results written to: $RESULTS_DIR"
