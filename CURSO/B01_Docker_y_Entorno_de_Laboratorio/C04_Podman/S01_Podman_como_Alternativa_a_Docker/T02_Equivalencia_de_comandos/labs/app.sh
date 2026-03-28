#!/bin/sh

echo "Container runtime: $(cat /proc/1/cgroup 2>/dev/null | head -1 || echo 'unknown')"
echo "Hostname: $(hostname)"
echo "User: $(id)"
