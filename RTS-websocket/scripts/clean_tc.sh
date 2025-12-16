#!/bin/bash
set -e

if [[ $EUID -ne 0 ]]; then echo "Error: Please run as root (sudo)."; exit 1; fi

# Auto-detect interface or use argument
DEFAULT_IFACE=$(ip route | grep default | awk '{print $5}' | head -n1)
INTERFACE=${1:-$DEFAULT_IFACE}

if [[ -z "$INTERFACE" ]]; then
    echo "Error: Could not detect network interface."
    echo "Usage: sudo $0 <interface_name>"
    exit 1
fi

echo "[*] Removing all Traffic Control rules on interface: $INTERFACE"

# Delete root qdisc
tc qdisc del dev $INTERFACE root 2>/dev/null || true

echo "[*] Interface $INTERFACE is now back to full speed."
