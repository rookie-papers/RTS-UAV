#!/bin/bash
set -e

# ================= 0. Permission & Configuration Check =================
if [[ $EUID -ne 0 ]]; then echo "Error: Please run as root (sudo)."; exit 1; fi

# Load configuration file
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CONFIG_FILE="$SCRIPT_DIR/config.env"

if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
    echo "[Config] Loaded config file: $CONFIG_FILE"
    echo "[Config] Planned node count: $NUM_UAV"
else
    echo "Error: Configuration file not found: $CONFIG_FILE"; exit 1
fi

# Ensure NUM_UAV is defined
if [[ -z "$NUM_UAV" ]]; then echo "Error: NUM_UAV is undefined in the config file."; exit 1; fi

# ================= 1. Batch Startup Logic =================
echo "[*] Starting $NUM_UAV UAV nodes in the background..."

for i in $(seq 1 $NUM_UAV); do
    NS_NAME="UAV$i"

    # Print brief logs to avoid flooding the terminal
    echo " -> Starting $NS_NAME..."

    # Core startup command:
    # 1. ip netns exec $NS_NAME: Enter the corresponding network namespace
    # 2. ./UAV_netSim: Run your program (Assuming execution from build dir)
    # 3. > /dev/null 2>&1: Discard output (Prevents terminal freeze due to mixed logs from 64 processes)
    # 4. &: Run in background
    ip netns exec $NS_NAME ./UAV_netSim > /dev/null 2>&1 &

    # Slight sleep to prevent CPU spikes or packet loss from instantaneous high concurrency
    sleep 0.05
done

# ================= 2. Completion Prompt =================
echo ""
echo "=== All UAV members started (Total: $NUM_UAV) ==="
echo "Hint: Use 'ps -ef | grep UAV_netSim' to check running status."
echo "Hint: Use 'sudo ./scripts/stop_all.sh' to stop all nodes."