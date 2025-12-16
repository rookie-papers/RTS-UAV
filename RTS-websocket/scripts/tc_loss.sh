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

# ================= 1. Interface Selection =================
# Try to detect the default interface (the one with internet access)
DEFAULT_IFACE=$(ip route | grep default | awk '{print $5}' | head -n1)

# Allow user to override via argument: sudo ./tc_loss.sh wlan0
INTERFACE=${1:-$DEFAULT_IFACE}

if [[ -z "$INTERFACE" ]]; then
    echo "Error: Could not detect network interface. Please specify it manually."
    echo "Usage: sudo $0 <interface_name>"
    exit 1
fi

# ================= 2. Parameter Validation =================
if [[ -z "$NET_LOSS" ]]; then echo "Error: NET_LOSS is undefined in config."; exit 1; fi

echo "[Config] Loaded config file: $CONFIG_FILE"
echo "[Target] Interface: $INTERFACE"
echo "[Params] Packet Loss: $NET_LOSS"
echo "----------------------------------------------------------------"
echo "WARNING: High packet loss (>10%) may disconnect your SSH session!"
echo "Clean all network rules use: sudo ./scripts/clean_tc.sh"
echo "----------------------------------------------------------------"

# ================= 3. Apply Rules =================

# 1. Clean existing rules (Reset to default)
echo "[*] Cleaning old rules on $INTERFACE..."
tc qdisc del dev $INTERFACE root 2>/dev/null || true

# 2. Apply Packet Loss (Netem)
echo "[*] Applying Network Emulation (Loss: $NET_LOSS)..."
# Quotes added to prevent parsing errors (e.g. if value is "10%")
tc qdisc add dev $INTERFACE root netem loss "$NET_LOSS"

echo "[*] Configuration Complete. Verify with: tc qdisc show dev $INTERFACE"