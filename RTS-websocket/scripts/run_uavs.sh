#!/bin/bash

# Get the script directory to ensure config.env can be found
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CONFIG_FILE="$SCRIPT_DIR/config.env"

# ================= 1. Load Configuration =================
if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
    echo "[Config] Loaded config file: $CONFIG_FILE"
else
    echo "Error: Config file not found: $CONFIG_FILE"
    exit 1
fi

# ================= 2. Check Configuration Items =================
# If NUM_UAV is undefined or empty, exit with error immediately
if [[ -z "$NUM_UAV" ]]; then
    echo "Error: 'NUM_UAV' is undefined in the config file."
    exit 1
fi

# ================= 3. Startup Logic =================
BASE_PORT=8002

echo "[*] Starting $NUM_UAV UAV nodes (Base Port: $BASE_PORT)..."

for ((i=0; i<NUM_UAV; i++)); do
    PORT=$((BASE_PORT + i))
    echo " -> Starting UAV $i on port $PORT ..."

    # Run in background
    ./UAV_exec $PORT &
    sleep 0.05
done

echo "All $NUM_UAV UAVs started."