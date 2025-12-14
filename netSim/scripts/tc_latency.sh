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
# Mandatory parameters: Node count and base delay
if [[ -z "$NUM_UAV" ]]; then echo "Error: NUM_UAV is undefined."; exit 1; fi
if [[ -z "$NET_DELAY" ]]; then echo "Error: NET_DELAY is undefined."; exit 1; fi

# Optional parameter: Jitter (Default to 0ms if undefined)
# Syntax explanation: ${VAR:-DEFAULT} uses DEFAULT if VAR is undefined or empty
NET_JITTER=${NET_JITTER:-"0ms"}

echo "[Config] Loaded config file: $CONFIG_FILE"
echo "[Config] Target Nodes: $NUM_UAV | Delay Settings: $NET_DELAY (Jitter: $NET_JITTER)"

# ================= 2. Define Helper Function =================
function clean_tc() {
    # Attempt to delete root qdisc, suppress errors (e.g., if rules don't exist)
    ip netns exec $1 tc qdisc del dev $2 root 2>/dev/null || true
}

echo "[*] Configuring Scenario 1: High Latency [Delay: $NET_DELAY, Jitter: $NET_JITTER]..."

# ================= 3. Configure UAV Members =================
for i in $(seq 1 $NUM_UAV); do
    NS_NAME="UAV$i"
    DEV_NAME="veth-uav$i-ns"

    clean_tc $NS_NAME $DEV_NAME
    # Quotes are added to variable references to prevent parsing errors if parameters contain spaces
    ip netns exec $NS_NAME tc qdisc add dev $DEV_NAME root netem delay "$NET_DELAY" "$NET_JITTER"
done
echo " -> Completed delay setup for $NUM_UAV normal nodes."

# ================= 4. Configure Core Nodes =================

# --- Configure UAVh ---
clean_tc "UAVh" "veth-uavh-ns"
echo " -> Setting UAVh ($NET_DELAY ± $NET_JITTER)"
ip netns exec UAVh tc qdisc add dev veth-uavh-ns root netem delay "$NET_DELAY" "$NET_JITTER"

# --- Configure Verifier ---
clean_tc "Verifier" "veth-vf-ns"
echo " -> Setting Verifier ($NET_DELAY ± $NET_JITTER)"
ip netns exec Verifier tc qdisc add dev veth-vf-ns root netem delay "$NET_DELAY" "$NET_JITTER"

echo "[*] Scenario 1 (High Latency) Configuration Complete."