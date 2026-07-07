#define DUCKDB_EXTENSION_MAIN

#include "query_limiter_extension.hpp"

#include "duckdb/common/exception.hpp"
#include "duckdb/common/limits.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/main/client_context.hpp"
#include "duckdb/main/config.hpp"
#include "duckdb/main/extension/extension_loader.hpp"
#include "duckdb/optimizer/optimizer_extension.hpp"
#include "duckdb/planner/operator/logical_get.hpp"

namespace duckdb {

namespace {

constexpr const char *MAX_ROWS_TO_SCAN_SETTING = "max_rows_to_scan";
constexpr const char *UNKNOWN_POLICY_SETTING = "max_rows_to_scan_unknown";
constexpr const char *UNKNOWN_POLICY_ALLOW = "allow";
constexpr const char *UNKNOWN_POLICY_REJECT = "reject";

struct ScanEstimate {
	idx_t rows = 0;
	idx_t unknown_scans = 0;
};

idx_t GetMaxRowsToScan(ClientContext &context) {
	Value value;
	if (!context.TryGetCurrentSetting(MAX_ROWS_TO_SCAN_SETTING, value) || value.IsNull()) {
		return 0;
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

void EnforceMaxRowsToScan(OptimizerExtensionInput &input, unique_ptr<LogicalOperator> &plan) {
	auto max_rows_to_scan = GetMaxRowsToScan(input.context);
	if (max_rows_to_scan == 0 || !plan) {
		return;
	}

	auto estimate = EstimateRowsToScan(input.context, *plan);
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

void RegisterSettings(DBConfig &config) {
	config.AddExtensionOption(MAX_ROWS_TO_SCAN_SETTING,
	                          "Reject queries before execution when estimated table-scan rows exceed this value. "
	                          "Set to 0 to disable.",
	                          LogicalType::UBIGINT, Value::UBIGINT(0));
	config.AddExtensionOption(
	    UNKNOWN_POLICY_SETTING,
	    "Policy for scans without row estimates when max_rows_to_scan is enabled: allow or reject.",
	    LogicalType::VARCHAR, Value(UNKNOWN_POLICY_ALLOW), ValidateUnknownPolicy);
}

void RegisterOptimizer(DBConfig &config) {
	OptimizerExtension extension;
	extension.optimize_function = EnforceMaxRowsToScan;
	OptimizerExtension::Register(config, std::move(extension));
}

void LoadInternal(ExtensionLoader &loader) {
	auto &config = DBConfig::GetConfig(loader.GetDatabaseInstance());
	RegisterSettings(config);
	RegisterOptimizer(config);
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
