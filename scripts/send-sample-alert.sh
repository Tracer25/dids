#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-9000}"

JSON='{"agent_id":"demo-agent","ts":"2026-03-01T12:00:00Z","rule":"SSH_FAILED_PASSWORD","severity":3,"src":"/var/log/auth.log","line":"Failed password for root from 10.0.0.2"}'

printf '%s\n' "$JSON" | nc "$HOST" "$PORT"
echo "Sent sample alert to ${HOST}:${PORT}"
