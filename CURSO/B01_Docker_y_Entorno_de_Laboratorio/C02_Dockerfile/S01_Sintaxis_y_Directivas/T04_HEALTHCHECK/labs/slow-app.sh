#!/bin/sh
echo "App starting (slow initialization)..."
sleep 12
touch /tmp/healthy
echo "App ready after slow init (health marker created)"
while true; do sleep 1; done
