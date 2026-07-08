# Query Limiter

Query Limiter is a DuckDB extension that rejects queries before physical execution when DuckDB estimates that table scans would exceed a configured row budget.

DuckDB already provides resource controls such as `max_memory`, `max_temp_directory_size`, and `threads`. Those settings are still useful, but they control execution resources rather than the amount of input data a query is expected to scan. A query can return only a few rows while still requiring a very large table scan, for example `SELECT * FROM big_table LIMIT 1`.

Query Limiter adds a different guardrail: a pre-execution scan budget. It inspects DuckDB's optimized logical plan, sums the estimated rows for table scans, and rejects the query **before** physical execution if the estimate is above `max_rows_to_scan`, so users don't pay their time and $$ on expensive queries.

## Usage

```sql
FORCE INSTALL query_limiter FROM community;
LOAD query_limiter;

SET max_rows_to_scan = 1000000;
SELECT * FROM my_large_table;
```

`max_rows_to_scan` is the maximum estimated number of input rows that table scans may read for a query. If DuckDB estimates that the query would scan more rows, Query Limiter rejects it before physical execution:

```text
Query rejected by max_rows_to_scan: estimated rows to scan is 1000001, limit is 1000000
```

By default, `max_rows_to_scan` is set to the maximum `idx_t` value, so queries are effectively unrestricted unless you configure a smaller budget.

The limit applies to estimated input rows scanned, not output rows. `LIMIT`, filters, and aggregations can reduce the result size without reducing the amount of data DuckDB expects to scan.

## Unknown Estimates

Some table functions do not expose a row estimate. By default Query Limiter allows those scans:

```sql
SET max_rows_to_scan_unknown = 'allow';
```

To fail queries that contain scans without estimates, use:

```sql
SET max_rows_to_scan_unknown = 'reject';
```

When rejected, the error reports how many scans did not provide an estimate.

## Logging

Enable DuckDB debug logging to see the estimated rows Query Limiter calculated for each planned query:

```sql
CALL enable_logging(storage='shell_log_storage', level='DEBUG');
```

With shell log storage, logs are printed directly in the DuckDB CLI. For example:

```sql
LOAD query_limiter;
CALL enable_logging(storage='shell_log_storage', level='DEBUG');

CREATE TABLE limiter_large AS SELECT i FROM range(100) tbl(i);
SELECT sum(i) FROM limiter_large;
```

The CLI will include a debug message like:

```text
DEBUG:
estimated rows to scan: 100, unknown scans: 0, limit: 18446744073709551615
```

`SELECT count(*) FROM limiter_large` may not show a 100-row scan because DuckDB can answer some counts from table metadata. Use a query that reads column data, such as `SELECT sum(i) FROM limiter_large`, when checking scan estimates manually.

## Limitations

Query Limiter depends on cardinality estimates exposed by DuckDB table scans and table functions. If a scan does not provide an estimate, the `max_rows_to_scan_unknown` setting controls whether the query is allowed or rejected.

The extension runs after planning and before physical execution. It prevents the data scan itself, but planning and binding may still need metadata access for some external sources.
