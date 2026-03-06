#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
docker compose up -d --build

echo "Stack is starting. Endpoints:"
echo "  Grafana:    http://localhost:3000 (admin/admin)"
echo "  Prometheus: http://localhost:9090"
echo "  ClickHouse: http://localhost:8123"
echo "  Server TCP: localhost:9000"
echo "  Metrics:    http://localhost:9464/metrics"
