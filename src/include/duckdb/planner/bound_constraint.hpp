//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/planner/bound_constraint.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/common.hpp"
#include "duckdb/parser/constraint.hpp"
#include "duckdb/common/serializer.hpp"
#include "duckdb/common/exception.hpp"

namespace duckdb {
//! Bound equivalent of Constraint
class BoundConstraint {
public:
	explicit BoundConstraint(ConstraintType type) : type(type) {};
	virtual ~BoundConstraint() {
	}

	void Serialize(Serializer &serializer) const {
		serializer.Write(type);
	}

	static unique_ptr<BoundConstraint> Deserialize(Deserializer &source) {
		return make_uniq<BoundConstraint>(source.Read<ConstraintType>());
	}

	ConstraintType type;

public:
	template <class TARGET>
	TARGET &Cast() {
		if (type != TARGET::TYPE) {
			throw InternalException("Failed to cast constraint to type - bound constraint type mismatch");
		}
		return (TARGET &)*this;
	}

	template <class TARGET>
	const TARGET &Cast() const {
		if (type != TARGET::TYPE) {
			throw InternalException("Failed to cast constraint to type - bound constraint type mismatch");
		}
		return (const TARGET &)*this;
	}
};
} // namespace duckdb
