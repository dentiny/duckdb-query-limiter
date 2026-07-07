# Query Limiter

This DuckDB extension rejects queries before physical execution when DuckDB estimates that table scans would exceed a configured row budget.

## Usage

```sql
LOAD query_limiter;
SET max_rows_to_scan = 1000000;

SELECT * FROM my_large_table;
```

Set `max_rows_to_scan` to `0` to disable the guard. This is the default.

When a scan does not provide a row estimate, the `max_rows_to_scan_unknown` setting controls the behavior:

```sql
SET max_rows_to_scan_unknown = 'allow';
SET max_rows_to_scan_unknown = 'reject';
```

The limit applies to estimated input rows scanned, not output rows. For example, `SELECT * FROM big_table LIMIT 1` can still be rejected if the table scan estimate exceeds the configured budget.

## Limitations

The guard depends on cardinality estimates exposed by DuckDB table functions. It prevents physical scan execution, but planning and binding may still need metadata access for some external sources.

## Building

```sh
make
```

The main binaries are:

```sh
./build/release/duckdb
./build/release/test/unittest
./build/release/extension/query_limiter/query_limiter.duckdb_extension
```

## Testing

```sh
make test
```
