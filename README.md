# DIDS Observability + Streaming Stack

This repo now includes a runnable stack that adds:
- OpenTelemetry Collector (metrics pipeline)
- Kafka (Redpanda-compatible)
- ClickHouse analytics storage
- Prometheus + Grafana monitoring

## What changed

### Server code
- Added Prometheus metrics endpoint on `:9464`.
- Added Kafka publish path for each valid alert line.
- Kept existing in-memory aggregator flow unchanged.

Environment variables used by `common/server/server`:
- `KAFKA_BROKERS` (example: `redpanda:9092`)
- `KAFKA_TOPIC` (default: `alerts.raw`)

CLI usage:
- `./server [port] [summary_interval_seconds] [metrics_port]`

### Infra
- `docker-compose.yml` starts:
  - `redpanda` (Kafka API)
  - `clickhouse`
  - `clickhouse-init` (creates DB/tables/materialized view)
  - `dids-server`
  - `otel-collector`
  - `prometheus`
  - `grafana`

ClickHouse ingests from Kafka using:
- `dids.kafka_alerts_raw` (Kafka engine table)
- `dids.mv_alerts_to_merge_tree` (materialized view)
- `dids.alerts` (MergeTree table)

## Run it

1. Start stack

```bash
./scripts/stack-up.sh
```

2. Send a sample alert

```bash
./scripts/send-sample-alert.sh
```

3. Verify metrics

```bash
curl -s http://localhost:9464/metrics
```

4. Verify ClickHouse ingestion

```bash
curl -s 'http://localhost:8123/?query=SELECT%20count()%20FROM%20dids.alerts'
```

5. Open dashboards
- Grafana: `http://localhost:3000` (`admin/admin`)
- Prometheus: `http://localhost:9090`

## Useful ClickHouse queries

```sql
SELECT rule_name, count() AS c
FROM dids.alerts
GROUP BY rule_name
ORDER BY c DESC;
```

```sql
SELECT agent_id, count() AS c
FROM dids.alerts
GROUP BY agent_id
ORDER BY c DESC;
```

```sql
SELECT toStartOfMinute(ts) AS minute, count() AS c
FROM dids.alerts
GROUP BY minute
ORDER BY minute DESC
LIMIT 60;
```

## Stop stack

```bash
./scripts/stack-down.sh
```
