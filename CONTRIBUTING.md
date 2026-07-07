# Contributing

## Did You Find A Bug?

* **Ensure the bug was not already reported** by searching under [Issues](https://github.com/dentiny/duckdb-query-limiter/issues).
* If you cannot find an open issue addressing the problem, [open a new one](https://github.com/dentiny/duckdb-query-limiter/issues/new/choose). Include a clear title and description, relevant environment details, and a minimal SQL or code sample that demonstrates the behavior.

## Did You Write A Patch That Fixes A Bug?

* Great!
* If possible, add a test case to make sure the issue does not occur again.
* Run the code formatter with `make format-all`.
* Open a new GitHub pull request with the patch.
* Ensure the PR description clearly describes the problem and solution. Include the relevant issue number if applicable.

## Pull Requests

* Do not commit or push directly to the main branch. Create a branch or fork and file a pull request.
* Keep branches up to date with main.
* Submit smaller pull requests when possible.
* Please do not open draft pull requests. Use issues or discussion topics for early design discussion.
* We reserve full and final discretion over whether or not we will merge a pull request. Adhering to these guidelines is not a complete guarantee that your pull request will be merged.

## Building

* Install `ccache` to improve compilation speed.
* To pull latest dependencies, run `git submodule update --init --recursive`.
* To build the project, run `CMAKE_BUILD_PARALLEL_LEVEL=$(nproc) make reldebug`.

## Testing

* To run the focused SQL test, run `./build/reldebug/test/unittest test/sql/query_limiter.test`.
* To run all SQL tests, run `make test` or `make test_debug`.

## Formatting

* Use tabs for indentation and spaces for alignment.
* Lines should not exceed 120 columns.
* Use `make format-all` to run the formatter.

## DuckDB C++ Guidelines

* Do not use `malloc`; prefer smart pointers. Keywords `new` and `delete` are a code smell.
* Strongly prefer `unique_ptr` over `shared_ptr`; use `shared_ptr` only when necessary.
* Use `const` whenever possible.
* Do not import namespaces, e.g. `using std`.
* Extension code should be part of the `duckdb` namespace. Put file-local helpers in an anonymous namespace.
* When overriding a virtual method, avoid repeating `virtual` and always use `override` or `final`.
* Use `[u]int(8|16|32|64)_t` instead of `int`, `long`, `uint`, etc. Use `idx_t` instead of `size_t` for offsets, indices, and counts.
* Prefer references over pointers as arguments.
* Use const references for non-trivial arguments such as `std::vector`.
* Use C++11 for loops when possible: `for (const auto &item : items) { ... }`.
* Use braces for `if` statements and loops. Avoid single-line conditionals, especially nested ones.
* Avoid unnamed magic numbers. Use named variables stored in a `constexpr`.
* Return early to avoid deeply nested branches.
* Do not include commented-out code blocks in pull requests.

## Error Handling

* Use exceptions only when an error terminates a query, such as parser errors or invalid settings.
* For expected recoverable cases, prefer return values over exceptions.
* Add test cases that trigger exceptions when practical. If an exception cannot be triggered with a test case, consider whether it should be an assertion.
* Use `D_ASSERT` for internal invariants. Assert should never be triggered by user input.
* Assert liberally, but make it clear with nearby context what went wrong.

## Naming Conventions

* Choose descriptive names. Avoid single-letter variable names.
* Files: lowercase separated by underscores, e.g. `query_limiter_extension.cpp`.
* Types: CamelCase starting with uppercase, e.g. `QueryLimiterExtension`.
* Variables: lowercase separated by underscores, e.g. `max_rows_to_scan`.
* Functions: CamelCase starting with uppercase, e.g. `GetMaxRowsToScan`.
* Avoid `i`, `j`, etc. in nested loops. Prefer descriptive names such as `column_idx`.

## Release Process

Extension binaries should be built and tested for the DuckDB platforms targeted by the release.

Use Linux amd64 musl as an example:

```sh
git clone git@github.com:duckdb/extension-ci-tools.git
cd extension-ci-tools/docker/linux_amd64_musl
docker build -t duckdb-ci-linux-amd64-musl .
docker run -it duckdb-ci-linux-amd64-musl
```

Inside the container:

```sh
git clone https://github.com/dentiny/duckdb-query-limiter.git
cd duckdb-query-limiter
git submodule update --init --recursive
CMAKE_BUILD_PARALLEL_LEVEL=$(nproc) make
```

See the [DuckDB extension-ci-tools docker directory](https://github.com/duckdb/extension-ci-tools/tree/main/docker) for supported environments.

## Update DuckDB Version

Community extensions should track released DuckDB versions.

Steps to update DuckDB and extension-ci-tools:

```sh
cd duckdb
git checkout tags/vX.Y.Z
cd ../extension-ci-tools
git checkout vX.Y.Z
cd ..
git add duckdb extension-ci-tools
CMAKE_BUILD_PARALLEL_LEVEL=10 make reldebug
./build/reldebug/test/unittest test/sql/query_limiter.test
```
