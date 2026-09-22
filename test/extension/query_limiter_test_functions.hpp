#pragma once

namespace duckdb {

class ExtensionLoader;

void RegisterQueryLimiterTestFunctions(ExtensionLoader &loader);

} // namespace duckdb
