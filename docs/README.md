# Query Limiter

Query Limiter is a DuckDB extension that rejects queries before physical execution when DuckDB estimates that table scans would exceed a configured row budget.

## Usage

```sql
FORCE INSTALL query_limiter FROM community;
LOAD query_limiter;

SET max_rows_to_scan = 1000000;
-- Query will fail if estimated rows to scan exceeds the limit.
SELECT * FROM my_large_table;
```

By default, `max_rows_to_scan` is set to the maximum `idx_t` value, so queries are effectively unrestricted unless you configure a smaller budget.

When a scan does not provide a row estimate, configure the unknown-estimate policy:

```sql
SET max_rows_to_scan_unknown = 'allow';
SET max_rows_to_scan_unknown = 'reject';
```

The limit applies to estimated input rows scanned, not output rows. For example, `SELECT * FROM big_table LIMIT 1` can still be rejected if the table scan estimate exceeds the configured budget.

## Limitations

The guard depends on cardinality estimates exposed by DuckDB table functions. It prevents physical scan execution, but planning and binding may still need metadata access for some external sources.
