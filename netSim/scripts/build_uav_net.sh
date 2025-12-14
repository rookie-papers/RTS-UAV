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
    echo "[Config] Target Nodes: $NUM_UAV | Network Settings: TA=$NET_TA, VF=$NET_VF, Swarm=$NET_SWARM"
else
    echo "Error: Configuration file not found: $CONFIG_FILE"; exit 1
fi

# Ensure NUM_UAV is defined in the config file
if [[ -z "$NUM_UAV" ]]; then echo "Error: NUM_UAV is undefined in the config file."; exit 1; fi


# ================= 1. Cleanup Old Environment =================
echo "[*] Cleaning up old environment..."
# Force cleanup: Delete namespaces first, then residual veths and bridges. Ignore errors.
ip netns list | awk '{print $1}' | xargs -r -n1 sudo ip netns delete >/dev/null 2>&1 || true
ip link show | grep veth | awk -F': ' '{print $2}' | cut -d'@' -f1 | xargs -r -n1 sudo ip link delete >/dev/null 2>&1 || true
ip link delete br-swarm >/dev/null 2>&1 || true

echo "[*] Cleanup finished. Starting build..."

# ================= 2. Core Settings =================
echo "[*] Enabling Kernel IP Forwarding..."
sysctl -w net.ipv4.ip_forward=1 > /dev/null

# Define Subnets (Can be moved to config.env, keeping defaults here)
NET_TA=${NET_TA:-"10.0.10"}
NET_VF=${NET_VF:-"10.0.20"}
NET_SWARM=${NET_SWARM:-"10.0.30"}

# ================= 3. Build Zone A: TA =================
echo "[+] Building Zone A (TA Server)..."
ip netns add TA
ip link add veth-ta type veth peer name veth-ta-ns
ip link set veth-ta-ns netns TA
ip addr add ${NET_TA}.1/24 dev veth-ta
ip link set veth-ta up
ip netns exec TA ip addr add ${NET_TA}.2/24 dev veth-ta-ns
ip netns exec TA ip link set veth-ta-ns up
ip netns exec TA ip link set lo up
ip netns exec TA ip route add default via ${NET_TA}.1

# ================= 4. Build Zone B: Verifier =================
echo "[+] Building Zone B (Verifier)..."
ip netns add Verifier
ip link add veth-vf type veth peer name veth-vf-ns
ip link set veth-vf-ns netns Verifier
ip addr add ${NET_VF}.1/24 dev veth-vf
ip link set veth-vf up
ip netns exec Verifier ip addr add ${NET_VF}.2/24 dev veth-vf-ns
ip netns exec Verifier ip link set veth-vf-ns up
ip netns exec Verifier ip link set lo up
ip netns exec Verifier ip route add default via ${NET_VF}.1

# ================= 5. Build Zone C: Swarm =================
echo "[+] Building Zone C Infrastructure (br-swarm)..."
ip link add name br-swarm type bridge
ip addr add ${NET_SWARM}.1/24 dev br-swarm
ip link set br-swarm up

# --- UAVh (Cluster Head) ---
echo "[+] Attaching UAVh..."
ip netns add UAVh
ip link add veth-uavh type veth peer name veth-uavh-ns
ip link set veth-uavh master br-swarm
ip link set veth-uavh up
ip link set veth-uavh-ns netns UAVh
ip netns exec UAVh ip addr add ${NET_SWARM}.2/24 dev veth-uavh-ns
ip netns exec UAVh ip link set veth-uavh-ns up
ip netns exec UAVh ip link set lo up
ip netns exec UAVh ip route add default via ${NET_SWARM}.1

# --- UAV Members ---
echo "[+] Attaching $NUM_UAV UAV Member nodes..."
for i in $(seq 1 $NUM_UAV); do
    NS_NAME="UAV$i"
    IP_ADDR="${NET_SWARM}.$((100+i))"
    VETH_HOST="veth-uav$i"
    VETH_NS="veth-uav$i-ns"

    ip netns add $NS_NAME
    ip link add $VETH_HOST type veth peer name $VETH_NS
    ip link set $VETH_HOST master br-swarm
    ip link set $VETH_HOST up
    ip link set $VETH_NS netns $NS_NAME
    ip netns exec $NS_NAME ip addr add ${IP_ADDR}/24 dev $VETH_NS
    ip netns exec $NS_NAME ip link set $VETH_NS up
    ip netns exec $NS_NAME ip link set lo up
    ip netns exec $NS_NAME ip route add default via ${NET_SWARM}.1
done

# ================= Completion Report =================
echo ""
echo "=== Network Build Complete (Nodes: $NUM_UAV) ==="
echo "=== Experiment Launch Steps ==="
echo "0. Configure Network Simulation:  sudo ./scripts/tc_latency.sh (Auto-reads config)"
echo "1. Start TA Server:               sudo ip netns exec TA ./TA_netSim"
echo "2. Start UAV Members:             sudo ./scripts/run_uavs.sh (Auto-reads config)"
echo "3. Start UAVh (Cluster Head):     sudo ip netns exec UAVh ./UAVh_netSim"
echo "4. Start Verifier:                sudo ip netns exec Verifier ./Verifier_netSim"