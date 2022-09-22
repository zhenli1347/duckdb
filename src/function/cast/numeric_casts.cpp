#include "duckdb/function/cast/default_casts.hpp"
#include "duckdb/function/cast/vector_cast_helpers.hpp"
#include "duckdb/common/operator/string_cast.hpp"
#include "duckdb/common/operator/numeric_cast.hpp"

namespace duckdb {

template <class SRC>
static BoundCastInfo InternalNumericCastSwitch(const LogicalType &source, const LogicalType &target) {
	// now switch on the result type
	switch (target.id()) {
	case LogicalTypeId::BOOLEAN:
		return &VectorCastHelpers::TryCastLoop<SRC, bool, duckdb::NumericTryCast>;
	case LogicalTypeId::TINYINT:
		return &VectorCastHelpers::TryCastLoop<SRC, int8_t, duckdb::NumericTryCast>;
	case LogicalTypeId::SMALLINT:
		return &VectorCastHelpers::TryCastLoop<SRC, int16_t, duckdb::NumericTryCast>;
	case LogicalTypeId::INTEGER:
		return &VectorCastHelpers::TryCastLoop<SRC, int32_t, duckdb::NumericTryCast>;
	case LogicalTypeId::BIGINT:
		return &VectorCastHelpers::TryCastLoop<SRC, int64_t, duckdb::NumericTryCast>;
	case LogicalTypeId::UTINYINT:
		return &VectorCastHelpers::TryCastLoop<SRC, uint8_t, duckdb::NumericTryCast>;
	case LogicalTypeId::USMALLINT:
		return &VectorCastHelpers::TryCastLoop<SRC, uint16_t, duckdb::NumericTryCast>;
	case LogicalTypeId::UINTEGER:
		return &VectorCastHelpers::TryCastLoop<SRC, uint32_t, duckdb::NumericTryCast>;
	case LogicalTypeId::UBIGINT:
		return &VectorCastHelpers::TryCastLoop<SRC, uint64_t, duckdb::NumericTryCast>;
	case LogicalTypeId::HUGEINT:
		return &VectorCastHelpers::TryCastLoop<SRC, hugeint_t, duckdb::NumericTryCast>;
	case LogicalTypeId::FLOAT:
		return &VectorCastHelpers::TryCastLoop<SRC, float, duckdb::NumericTryCast>;
	case LogicalTypeId::DOUBLE:
		return &VectorCastHelpers::TryCastLoop<SRC, double, duckdb::NumericTryCast>;
	case LogicalTypeId::DECIMAL:
		return &VectorCastHelpers::ToDecimalCast<SRC>;
	case LogicalTypeId::JSON:
	case LogicalTypeId::VARCHAR:
		return &VectorCastHelpers::StringCast<SRC, duckdb::StringCast>;
	default:
		return DefaultCasts::TryVectorNullCast;
	}
}

BoundCastInfo DefaultCasts::NumericCastSwitch(BindCastInput &input, const LogicalType &source,
                                              const LogicalType &target) {
	switch (source.id()) {
	case LogicalTypeId::BOOLEAN:
		return InternalNumericCastSwitch<bool>(source, target);
	case LogicalTypeId::TINYINT:
		return InternalNumericCastSwitch<int8_t>(source, target);
	case LogicalTypeId::SMALLINT:
		return InternalNumericCastSwitch<int16_t>(source, target);
	case LogicalTypeId::INTEGER:
		return InternalNumericCastSwitch<int32_t>(source, target);
	case LogicalTypeId::BIGINT:
		return InternalNumericCastSwitch<int64_t>(source, target);
	case LogicalTypeId::UTINYINT:
		return InternalNumericCastSwitch<uint8_t>(source, target);
	case LogicalTypeId::USMALLINT:
		return InternalNumericCastSwitch<uint16_t>(source, target);
	case LogicalTypeId::UINTEGER:
		return InternalNumericCastSwitch<uint32_t>(source, target);
	case LogicalTypeId::UBIGINT:
		return InternalNumericCastSwitch<uint64_t>(source, target);
	case LogicalTypeId::HUGEINT:
		return InternalNumericCastSwitch<hugeint_t>(source, target);
	case LogicalTypeId::FLOAT:
		return InternalNumericCastSwitch<float>(source, target);
	case LogicalTypeId::DOUBLE:
		return InternalNumericCastSwitch<double>(source, target);
	default:
		throw InternalException("NumericCastSwitch called with non-numeric argument");
	}
}

} // namespace duckdb
