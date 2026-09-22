#include "query_limiter_test_functions.hpp"

#include "duckdb/common/types/data_chunk.hpp"
#include "duckdb/common/types/value.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/parser/parsed_data/create_table_function_info.hpp"

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

} // namespace

void RegisterQueryLimiterTestFunctions(ExtensionLoader &loader) {
	TableFunction function("query_limiter_unknown_estimate", {}, UnknownEstimateScan, UnknownEstimateScanBind,
	                       UnknownEstimateScanInit);
	CreateTableFunctionInfo info(std::move(function));
	info.on_conflict = OnCreateConflict::ALTER_ON_CONFLICT;

	FunctionDescription description;
	description.parameter_names = {};
	description.description = "Returns two integer rows without exposing a cardinality estimate to the query planner.";
	description.examples = {"SELECT * FROM query_limiter_unknown_estimate();"};
	description.categories = {"test", "query_limiter"};
	info.descriptions.push_back(std::move(description));

	loader.RegisterFunction(std::move(info));
}

} // namespace duckdb
