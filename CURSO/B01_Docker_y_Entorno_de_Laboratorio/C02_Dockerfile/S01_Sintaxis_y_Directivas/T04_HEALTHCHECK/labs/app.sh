#!/bin/sh
echo "App starting..."
sleep 2
touch /tmp/healthy
echo "App ready (health marker created)"
while true; do sleep 1; done
