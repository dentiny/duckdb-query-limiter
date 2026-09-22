#define DUCKDB_EXTENSION_MAIN

#include "query_limiter_extension.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/limits.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/execution/expression_executor_state.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/logging/logger.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/client_context_state.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/optimizer/optimizer_extension.hpp"
#include "duckdb/optimizer/optimizer.hpp"
#include "duckdb/parser/parsed_data/create_scalar_function_info.hpp"
#include "duckdb/parser/parser.hpp"
#include "duckdb/planner/planner.hpp"
#include "duckdb/planner/operator/logical_get.hpp"

namespace duckdb {

namespace {

//===--------------------------------------------------------------------===//
// Constants and State
//===--------------------------------------------------------------------===//

constexpr const char *MAX_ROWS_TO_SCAN_SETTING = "max_rows_to_scan";
constexpr const char *UNKNOWN_POLICY_SETTING = "max_rows_to_scan_unknown";
constexpr const char *ESTIMATE_FUNCTION = "query_limiter_estimate";
constexpr const char *CONTEXT_STATE_KEY = "query_limiter_context_state";
constexpr const char *UNKNOWN_POLICY_ALLOW = "allow";
constexpr const char *UNKNOWN_POLICY_REJECT = "reject";
constexpr idx_t DEFAULT_MAX_ROWS_TO_SCAN = NumericLimits<idx_t>::Maximum();

struct ScanEstimate {
	idx_t rows = 0;
	idx_t unknown_scans = 0;
};

struct QueryLimiterContextState : public ClientContextState {
	bool planning_estimate = false;
};

class EstimatePlanningGuard {
public:
	explicit EstimatePlanningGuard(ClientContext &context)
	    : state(context.registered_state->GetOrCreate<QueryLimiterContextState>(CONTEXT_STATE_KEY)) {
		if (state->planning_estimate) {
			throw InvalidInputException("%s cannot be nested", ESTIMATE_FUNCTION);
		}
		state->planning_estimate = true;
	}

	~EstimatePlanningGuard() {
		state->planning_estimate = false;
	}

private:
	shared_ptr<QueryLimiterContextState> state;
};

//===--------------------------------------------------------------------===//
// Settings
//===--------------------------------------------------------------------===//

idx_t GetMaxRowsToScan(ClientContext &context) {
	Value value;
	if (!context.TryGetCurrentSetting(MAX_ROWS_TO_SCAN_SETTING, value) || value.IsNull()) {
		return DEFAULT_MAX_ROWS_TO_SCAN;
	}
	return value.GetValue<idx_t>();
}

string GetUnknownPolicy(ClientContext &context) {
	Value value;
	if (!context.TryGetCurrentSetting(UNKNOWN_POLICY_SETTING, value) || value.IsNull()) {
		return UNKNOWN_POLICY_ALLOW;
	}
	return StringUtil::Lower(value.GetValue<string>());
}

void ValidateUnknownPolicy(ClientContext &, SetScope, Value &parameter) {
	auto policy = StringUtil::Lower(parameter.GetValue<string>());
	if (policy != UNKNOWN_POLICY_ALLOW && policy != UNKNOWN_POLICY_REJECT) {
		throw InvalidInputException("%s must be either '%s' or '%s'", UNKNOWN_POLICY_SETTING, UNKNOWN_POLICY_ALLOW,
		                            UNKNOWN_POLICY_REJECT);
	}
	parameter = Value(policy);
}

void RegisterSettings(DBConfig &config) {
	config.AddExtensionOption(MAX_ROWS_TO_SCAN_SETTING,
	                          "Reject queries before execution when estimated table-scan rows exceed this value. "
	                          "Defaults to the maximum idx_t value.",
	                          LogicalType::UBIGINT, Value::UBIGINT(DEFAULT_MAX_ROWS_TO_SCAN));
	config.AddExtensionOption(
	    UNKNOWN_POLICY_SETTING,
	    "Policy for scans without row estimates when max_rows_to_scan is enabled: allow or reject.",
	    LogicalType::VARCHAR, Value(UNKNOWN_POLICY_ALLOW), ValidateUnknownPolicy);
}

//===--------------------------------------------------------------------===//
// Scan Estimation
//===--------------------------------------------------------------------===//

void AddRowsWithSaturation(idx_t &target, idx_t rows) {
	if (NumericLimits<idx_t>::Maximum() - target < rows) {
		target = NumericLimits<idx_t>::Maximum();
		return;
	}
	target += rows;
}

ScanEstimate EstimateRowsToScan(ClientContext &context, LogicalOperator &op) {
	ScanEstimate result;
	if (op.type == LogicalOperatorType::LOGICAL_GET) {
		auto &get = op.Cast<LogicalGet>();
		if (get.function.cardinality) {
			auto cardinality = get.function.cardinality(context, get.bind_data.get());
			if (cardinality && cardinality->has_estimated_cardinality) {
				result.rows = cardinality->estimated_cardinality;
			} else {
				result.unknown_scans++;
			}
		} else {
			result.unknown_scans++;
		}
	}
	for (auto &child : op.children) {
		auto child_estimate = EstimateRowsToScan(context, *child);
		AddRowsWithSaturation(result.rows, child_estimate.rows);
		result.unknown_scans += child_estimate.unknown_scans;
	}
	return result;
}

//===--------------------------------------------------------------------===//
// Estimate Function
//===--------------------------------------------------------------------===//

ScanEstimate PlanAndEstimate(ClientContext &context, const string &query) {
	EstimatePlanningGuard guard(context);
	Parser parser(context.GetParserOptions());
	parser.ParseQuery(query);
	if (parser.statements.size() != 1) {
		throw InvalidInputException("%s requires exactly one SQL statement", ESTIMATE_FUNCTION);
	}

	Planner planner(context);
	planner.CreatePlan(std::move(parser.statements[0]));
	auto plan = std::move(planner.plan);
	D_ASSERT(plan);

	if (context.config.enable_optimizer && plan->RequireOptimizer()) {
		Optimizer optimizer(*planner.binder, context);
		plan = optimizer.Optimize(std::move(plan));
	}
	return EstimateRowsToScan(context, *plan);
}

LogicalType EstimateResultType() {
	return LogicalType::STRUCT({{"estimated_rows", LogicalType::UBIGINT}, {"unknown_scans", LogicalType::UBIGINT}});
}

void EstimateFunction(DataChunk &args, ExpressionState &state, Vector &result) {
	if (!state.HasContext()) {
		throw InvalidInputException("%s requires a client context", ESTIMATE_FUNCTION);
	}

	auto &context = state.GetContext();
	for (idx_t row = 0; row < args.size(); row++) {
		auto query = args.GetValue(0, row);
		if (query.IsNull()) {
			result.SetValue(row, Value(EstimateResultType()));
			continue;
		}
		auto estimate = PlanAndEstimate(context, query.GetValue<string>());
		result.SetValue(row, Value::STRUCT({{"estimated_rows", Value::UBIGINT(estimate.rows)},
		                                    {"unknown_scans", Value::UBIGINT(estimate.unknown_scans)}}));
	}
}

void RegisterFunctions(ExtensionLoader &loader) {
	ScalarFunction function(ESTIMATE_FUNCTION, {LogicalType::VARCHAR}, EstimateResultType(), EstimateFunction);
	function.SetVolatile();
	function.SetFallible();

	CreateScalarFunctionInfo info(std::move(function));
	FunctionDescription description;
	description.parameter_names = {"query"};
	description.description =
	    "Plans one SQL statement without executing it and returns its estimated table-scan rows and unknown scans.";
	description.examples = {"SELECT query_limiter_estimate('SELECT * FROM my_table');"};
	description.categories = {"query", "query_limiter"};
	info.descriptions.push_back(std::move(description));
	loader.RegisterFunction(std::move(info));
}

//===--------------------------------------------------------------------===//
// Optimizer Extension
//===--------------------------------------------------------------------===//

void EnforceMaxRowsToScan(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan) {
	auto state = input.context.registered_state->Get<QueryLimiterContextState>(CONTEXT_STATE_KEY);
	if (state && state->planning_estimate) {
		return;
	}
	auto max_rows_to_scan = GetMaxRowsToScan(input.context);
	if (!plan) {
		return;
	}

	auto estimate = EstimateRowsToScan(input.context, *plan);
	DUCKDB_LOG_DEBUG(input.context, StringUtil::Format("estimated rows to scan: %llu, unknown scans: %llu, limit: %llu",
	                                                   estimate.rows, estimate.unknown_scans, max_rows_to_scan));
	auto unknown_policy = GetUnknownPolicy(input.context);
	if (estimate.unknown_scans > 0 && unknown_policy == UNKNOWN_POLICY_REJECT) {
		throw InvalidInputException("Query rejected by %s: %llu table scan(s) do not provide a row scan estimate",
		                            MAX_ROWS_TO_SCAN_SETTING, estimate.unknown_scans);
	}
	if (estimate.rows > max_rows_to_scan) {
		throw InvalidInputException("Query rejected by %s: estimated rows to scan is %llu, limit is %llu",
		                            MAX_ROWS_TO_SCAN_SETTING, estimate.rows, max_rows_to_scan);
	}
}

void RegisterOptimizer(DBConfig &config) {
	OptimizerExtension extension;
	extension.optimize_function = EnforceMaxRowsToScan;
	OptimizerExtension::Register(config, std::move(extension));
}

//===--------------------------------------------------------------------===//
// Extension Loading
//===--------------------------------------------------------------------===//

void LoadInternal(ExtensionLoader &loader) {
	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
	RegisterSettings(config);
	RegisterOptimizer(config);
	RegisterFunctions(loader);
}

} // namespace

void QueryLimiterExtension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}

std::string QueryLimiterExtension::Name() {
	return "query_limiter";
}

std::string QueryLimiterExtension::Version() const {
#ifdef EXT_VERSION_QUERY_LIMITER
	return EXT_VERSION_QUERY_LIMITER;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(query_limiter, loader) {
	duckdb::LoadInternal(loader);
}
}
