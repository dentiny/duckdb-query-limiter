#define DUCKDB_EXTENSION_MAIN

#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"

namespace duckdb {

namespace {

struct UnknownEstimateScanState : public GlobalTableFunctionState {
	idx_t offset = 0;
};

unique_ptr<FunctionData> UnknownEstimateScanBind(ClientContext &, TableFunctionBindInput &, vector<LogicalType> &types,
                                                 vector<string> &names) {
	types.emplace_back(LogicalType::INTEGER);
	names.emplace_back("i");
	return nullptr;
}

unique_ptr<GlobalTableFunctionState> UnknownEstimateScanInit(ClientContext &, TableFunctionInitInput &) {
	return make_uniq<UnknownEstimateScanState>();
}

void UnknownEstimateScan(ClientContext &, TableFunctionInput &input, DataChunk &output) {
	auto &state = input.global_state->Cast<UnknownEstimateScanState>();
	idx_t count = 0;
	while (state.offset < 2 && count < STANDARD_VECTOR_SIZE) {
		output.SetValue(0, count++, Value::INTEGER(static_cast<int32_t>(state.offset)));
		state.offset++;
	}
	output.SetCardinality(count);
}

void LoadInternal(ExtensionLoader &loader) {
	TableFunction unknown_estimate("query_limiter_unknown_estimate", {}, UnknownEstimateScan, UnknownEstimateScanBind,
	                               UnknownEstimateScanInit);
	loader.RegisterFunction(std::move(unknown_estimate));
}

} // namespace

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(query_limiter_test, loader) {
	duckdb::LoadInternal(loader);
}
}
