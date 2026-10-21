# 0.2.0

## Changed

- Upgrade DuckDB to `v2.0.0` and pin extension-ci-tools to `fe955ed3a046b0324cac2c691e73d8d776a7d5cc`.
- Migrate table-function column names to DuckDB identifiers and use the v2.0 planner and vector APIs for scan estimates.
- Enforce standard C++17 and correct the distribution workflow extension name.

# 0.1.2

## Changed

- Update DuckDB and extension-ci-tools to `v1.5.6`.

## Added

- Add discoverable descriptions, examples, categories, and parameter names for registered DuckDB functions.

- Add `query_limiter_estimate(query)` to inspect estimated scan rows and unknown scans without executing the query ([#12]).

[#12]: https://github.com/dentiny/duckdb-query-limiter/pull/12

# 0.1.1

## Changed

- Upgrade DuckDB and extension-ci-tools to v1.5.5

# 0.1.0

## Added

- Initial release of Query Limiter extension ([#1])

[#1]: https://github.com/dentiny/duckdb-query-limiter/pull/1
[#10]: https://github.com/dentiny/duckdb-query-limiter/issues/10
