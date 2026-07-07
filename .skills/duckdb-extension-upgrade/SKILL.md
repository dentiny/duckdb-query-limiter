---
name: upgrade-duckdb-extension
description: Upgrade the duckdb-query-limiter extension to a new DuckDB release. Use when the user asks to upgrade DuckDB, bump the duckdb submodule, sync to a new DuckDB tag (e.g. v1.5.2), or update the duckdb / extension-ci-tools submodules together.
---

# Upgrade duckdb-query-limiter to a new DuckDB release

Two submodules must move together. Pin both to matching release tags, then build, run the SQL tests, and update docs if the upgrade changes commands or compatibility notes.

## Inputs

Before starting, confirm the target DuckDB version (e.g. `v1.5.2`). Everything else is derived from it.

## Workflow

Track these as a checklist; do not skip ahead:

- 1. Pin duckdb submodule to tags/$TARGET
- 2. Pin extension-ci-tools submodule to $TARGET (same tag)
- 3. Reconcile CMakeLists.txt EXTENSION_SOURCES if DuckDB extension build APIs changed
- 4. Build: CMAKE_BUILD_PARALLEL_LEVEL=10 make reldebug
- 5. Run SQL tests: `./build/reldebug/test/unittest test/sql/query_limiter.test`
- 6. Update docs if the target DuckDB version changes build, install, or runtime instructions

## Reference

This extension follows the standard DuckDB extension-template layout with `duckdb` and `extension-ci-tools` as submodules.
