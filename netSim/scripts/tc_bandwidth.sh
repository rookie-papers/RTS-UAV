#!/bin/bash
set -e

# ================= 0. Permission & Configuration Check =================
if [[ $EUID -ne 0 ]]; then echo "Error: Please run as root (sudo)."; exit 1; fi

# Load configuration file
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CONFIG_FILE="$SCRIPT_DIR/config.env"

if [ -f "$CONFIG_FILE" ]; then
    source "$CONFIG_FILE"
else
    echo "Error: Configuration file not found: $CONFIG_FILE"; exit 1
fi

# ================= 1. Parameter Validation & Defaults =================
if [[ -z "$NUM_UAV" ]]; then echo "Error: NUM_UAV is undefined."; exit 1; fi
if [[ -z "$NET_BANDWIDTH" ]]; then echo "Error: NET_BANDWIDTH is undefined."; exit 1; fi

# Set defaults (Use defaults if these parameters are missing in config to prevent errors)
NET_BURST=${NET_BURST:-"32kbit"}
NET_LATENCY=${NET_LATENCY:-"400ms"}

echo "[Config] Loaded config file: $CONFIG_FILE"
echo "[Config] Target Nodes: $NUM_UAV | Bandwidth Limit: $NET_BANDWIDTH (Burst: $NET_BURST, Latency: $NET_LATENCY)"

# ================= 2. Define Helper Function =================
function clean_tc() {
    ip netns exec $1 tc qdisc del dev $2 root 2>/dev/null || true
}

echo "[*] Configuring Scenario 2: Ultra-low Bandwidth (UAVh Upload Limit)..."

# ================= 3. Clean Old Rules =================
# Critical! Must clear netem delay on UAV nodes in case the latency script was run previously.
echo " -> Cleaning normal UAV node rules (Prevent residual latency)..."
for i in $(seq 1 $NUM_UAV); do
    clean_tc "UAV$i" "veth-uav$i-ns"
done

# Clean Verifier
clean_tc "Verifier" "veth-vf-ns"

# ================= 4. Set UAVh Core Limits =================
# Only limit UAVh egress bandwidth (Upload direction to Verifier)
clean_tc "UAVh" "veth-uavh-ns"

echo " -> Setting UAVh bandwidth limit: $NET_BANDWIDTH"

# Apply TBF (Token Bucket Filter) rules
ip netns exec UAVh tc qdisc add dev veth-uavh-ns root tbf \
    rate "$NET_BANDWIDTH" \
    burst "$NET_BURST" \
    latency "$NET_LATENCY"

echo "[*] Scenario 2 (Bandwidth Limit) Configuration Complete."