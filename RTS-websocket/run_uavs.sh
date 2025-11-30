#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: ./run_uavs.sh <num_uavs>"
    exit 1
fi

NUM=$1
BASE_PORT=8002

for ((i=0; i<NUM; i++)); do
    PORT=$((BASE_PORT + i))
    echo "Starting UAV $i on port $PORT ..."

    # 后台运行
    ./UAV_exec $PORT &
done

echo "All UAVs started."
