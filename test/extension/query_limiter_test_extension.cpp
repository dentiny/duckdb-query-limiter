#define DUCKDB_EXTENSION_MAIN

#include "duckdb/main/extension/extension_loader.hpp"
#include "query_limiter_test_functions.hpp"

namespace duckdb {

namespace {

void LoadInternal(ExtensionLoader &loader) {
	RegisterQueryLimiterTestFunctions(loader);
}

} // namespace

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(query_limiter_test, loader) {
	duckdb::LoadInternal(loader);
}
}
