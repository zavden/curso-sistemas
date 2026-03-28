#!/bin/sh

while true; do
    echo "App running — PORT=${PORT:-8080} ENV=${APP_ENV:-development}"
    sleep 5
done
