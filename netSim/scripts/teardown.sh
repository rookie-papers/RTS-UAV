#!/bin/bash

echo "[*] 1. Forcing termination of simulation processes..."
sudo pkill -9 -f _netSim 2>/dev/null || true
sleep 1

echo "[*] 2. Cleaning Network Namespaces..."
# Suppress error output, delete silently
sudo ip -all netns delete >/dev/null 2>&1 || true

# Fallback cleanup (in case 'ip -all' is not supported)
sudo ip netns list | awk '{print $1}' | xargs -r -n1 sudo ip netns delete >/dev/null 2>&1 || true

echo "[*] 3. Cleaning residual host devices..."
sudo ip link delete br-swarm >/dev/null 2>&1 || true

# Core optimization: Only delete interfaces that actually exist and suppress errors
# Filter specifically for veth interfaces
# 2>/dev/null discards "Cannot find device" noise
ip link show | grep veth | awk -F': ' '{print $2}' | cut -d'@' -f1 | \
xargs -r -n1 sudo ip link delete >/dev/null 2>&1 || true

echo "[*] Environment cleanup complete (Clean)."