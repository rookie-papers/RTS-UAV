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

# ================= 1. Parameter Validation =================
if [[ -z "$NUM_UAV" ]]; then echo "Error: NUM_UAV is undefined."; exit 1; fi
if [[ -z "$NET_LOSS" ]]; then echo "Error: NET_LOSS is undefined."; exit 1; fi

echo "[Config] Loaded config file: $CONFIG_FILE"
echo "[Config] Target Nodes: $NUM_UAV | Packet Loss Settings: $NET_LOSS"

# ================= 2. Define Helper Function =================
function clean_tc() {
    ip netns exec $1 tc qdisc del dev $2 root 2>/dev/null || true
}

echo "[*] Configuring Scenario 3: High Packet Loss (Internal Swarm Loss) [Loss: $NET_LOSS]..."

# ================= 3. Configure UAV Members =================
# Simulate loss of signature data reported by UAVs (Uplink)
for i in $(seq 1 $NUM_UAV); do
    NS_NAME="UAV$i"
    DEV_NAME="veth-uav$i-ns"

    clean_tc $NS_NAME $DEV_NAME
    # Use NET_LOSS from config
    ip netns exec $NS_NAME tc qdisc add dev $DEV_NAME root netem loss "$NET_LOSS"
done
echo " -> Packet loss rate set for $NUM_UAV UAV nodes."

# ================= 4. Configure Core Nodes =================

# --- Configure UAVh (Simulate command request loss) ---
clean_tc "UAVh" "veth-uavh-ns"
echo " -> Setting UAVh loss: $NET_LOSS"
ip netns exec UAVh tc qdisc add dev veth-uavh-ns root netem loss "$NET_LOSS"

# --- Clean Verifier (Keep external link perfect) ---
clean_tc "Verifier" "veth-vf-ns"
echo " -> Verifier network is perfect (Rules cleared)"

echo "[*] Scenario 3 (Packet Loss) Configuration Complete."