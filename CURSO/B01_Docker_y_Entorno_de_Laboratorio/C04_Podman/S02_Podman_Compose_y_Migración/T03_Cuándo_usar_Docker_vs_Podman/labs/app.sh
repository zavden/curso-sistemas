#!/bin/sh

echo "Tool: ${CONTAINER_TOOL:-unknown}"
echo "Hostname: $(hostname)"
echo "User: $(id -u)"
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d= -f2)"
