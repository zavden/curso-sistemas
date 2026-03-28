#!/bin/sh
echo "[entrypoint] Initializing..."
echo "[entrypoint] Arguments received: $@"

# Setup tasks
if [ -n "$APP_ENV" ]; then
    echo "[entrypoint] Environment: $APP_ENV"
fi

# exec replaces the shell with the command — CMD becomes PID 1
exec "$@"
