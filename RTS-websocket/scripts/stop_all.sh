#!/bin/bash

echo "[*] Terminating all UAV processes..."

# Use 'killall' instead of 'pkill -f' to avoid self-killing logic errors.
# -q: Quiet mode (don't complain if no process is found)
# -9: Force kill
sudo killall -9 -q UAV_exec
sudo killall -9 -q UAV_netSim

# Just in case, kill the original name too
sudo killall -9 -q _netSim

echo " -> Waiting for ports to release..."
sleep 1

# Check if any processes remain
# grep -v grep: Excludes the grep search itself from results
COUNT=$(ps -ef | grep -E "UAV_exec|UAV_netSim" | grep -v grep | wc -l)

if [ $COUNT -eq 0 ]; then
    echo "All UAV processes cleaned up. Ports released."
else
    echo "Warning: $COUNT processes remaining. Check manually: ps -ef | grep UAV_exec"
fi