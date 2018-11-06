
#include "execution/operator/physical_nested_loop_join.hpp"
#include "common/vector_operations/vector_operations.hpp"
#include "execution/expression_executor.hpp"

using namespace duckdb;
using namespace std;

PhysicalNestedLoopJoin::PhysicalNestedLoopJoin(
    unique_ptr<PhysicalOperator> left, unique_ptr<PhysicalOperator> right,
    vector<JoinCondition> cond, JoinType join_type)
    : PhysicalJoin(PhysicalOperatorType::NESTED_LOOP_JOIN, move(cond),
                   join_type) {
	children.push_back(move(left));
	children.push_back(move(right));
}

bool PhysicalNestedLoopJoin::CreateResult(DataChunk &left, size_t left_position,
                                          DataChunk &right, DataChunk &result,
                                          Vector &matches, bool is_last_chunk) {
	switch (type) {
	case JoinType::INNER: {
		// create a selection vector from the result
		result.SetSelectionVector(matches);
		if (result.size() > 0) {
			// we have elements in our result!
			// first duplicate the left side
			for (size_t i = 0; i < left.column_count; i++) {
				VectorOperations::Set(result.data[i],
				                      left.data[i].GetValue(left_position));
			}
			// use the selection vector we created on the right side
			for (size_t i = 0; i < right.column_count; i++) {
				// now create a reference to the vectors of the right chunk
				size_t chunk_entry = left.column_count + i;
				result.data[chunk_entry].Reference(right.data[i]);
				result.data[chunk_entry].sel_vector = result.sel_vector;
				result.data[chunk_entry].count = result.size();
			}
		}
		break;
	}
	case JoinType::ANTI: {
		// anti-join
		// we want to know if there are zero matches or not
		// check if this chunk has any matches
		Value match = Value::BOOLEAN(true);
		if (!VectorOperations::Contains(matches, match)) {
			// if there is a match, we skip this value
			return true;
		}
		// otherwise, we output it ONLY if it is the last chunk on the right
		// side
		if (is_last_chunk) {
			for (size_t i = 0; i < left.column_count; i++) {
				result.data[i].count = 1;
				result.data[i].sel_vector = nullptr;
				result.data[i].SetValue(0,
				                        left.data[i].GetValue(left_position));
			}
		}
		return false;
	}
	case JoinType::SEMI: {
		Value match = Value::BOOLEAN(true);
		// semi-join, check if there are any matches
		if (!VectorOperations::Contains(matches, match)) {
			// there is no match, check the next chunk
			return false;
		}

		// there is a match, output this tuple
		// we only output the left side, we don't care which tuple matched
		for (size_t i = 0; i < left.column_count; i++) {
			result.data[i].count = 1;
			result.data[i].sel_vector = nullptr;
			result.data[i].SetValue(0, left.data[i].GetValue(left_position));
		}
		return true;
	}
	default:
		throw NotImplementedException("Join type not supported!");
	}
	return false;
}

void PhysicalNestedLoopJoin::_GetChunk(ClientContext &context, DataChunk &chunk,
                                       PhysicalOperatorState *state_) {
	auto state =
	    reinterpret_cast<PhysicalNestedLoopJoinOperatorState *>(state_);
	chunk.Reset();

	if (type == JoinType::LEFT || type == JoinType::RIGHT ||
	    type == JoinType::OUTER) {
		throw NotImplementedException(
		    "Only inner/semi/anti joins supported for now!");
	}

	// first we fully materialize the right child, if we haven't done that yet
	if (state->right_chunks.column_count() == 0) {
		auto right_state = children[1]->GetOperatorState(state->parent);
		auto types = children[1]->GetTypes();

		DataChunk new_chunk;
		new_chunk.Initialize(types);
		do {
			children[1]->GetChunk(context, new_chunk, right_state.get());
			state->right_chunks.Append(new_chunk);
		} while (new_chunk.size() > 0);

		if (state->right_chunks.count == 0) {
			return;
		}
		// initialize the chunks for the join conditions
		vector<TypeId> left_types, right_types;
		for (auto &cond : conditions) {
			left_types.push_back(cond.left->return_type);
			right_types.push_back(cond.right->return_type);
		}
		state->left_join_condition.Initialize(left_types);
		state->right_join_condition.Initialize(right_types);
	}
	// now that we have fully materialized the right child
	// we have to perform the nested loop join

	do {
		chunk.Reset();
		// first check if we have to fetch a new chunk from the left child
		if (state->left_position >= state->child_chunk.size()) {
			// if we have exhausted the current left chunk, fetch a new one
			children[0]->GetChunk(context, state->child_chunk,
			                      state->child_state.get());
			if (state->child_chunk.size() == 0) {
				return;
			}
			state->left_position = 0;
			state->right_chunk = 0;

			// resolve the left join condition for the current chunk
			state->left_join_condition.Reset();
			ExpressionExecutor executor(state->child_chunk, context);
			executor.Execute(state->left_join_condition,
			                 [&](size_t i) { return conditions[i].left.get(); },
			                 conditions.size());
		}

		auto &left_chunk = state->child_chunk;
		auto &right_chunk = *state->right_chunks.chunks[state->right_chunk];

		// sanity check, this went wrong before
		left_chunk.Verify();
		right_chunk.Verify();

		// join the current row of the left relation with the current chunk
		// from the right relation
		state->right_join_condition.Reset();
		ExpressionExecutor executor(right_chunk, context);
		Vector final_result;
		for (size_t i = 0; i < conditions.size(); i++) {
			Vector &right_match = state->right_join_condition.data[i];
			// first resolve the join expression of the right side
			executor.ExecuteExpression(conditions[i].right.get(), right_match);
			// now perform the join for the current tuple
			// we retrieve one value from the left hand side
			Vector left_match(state->left_join_condition.data[i].GetValue(
			    state->left_position));

			Vector &l = conditions[i].flip ? right_match : left_match;
			Vector &r = conditions[i].flip ? left_match : right_match;

			Vector intermediate(TypeId::BOOLEAN, true, false);
			switch (conditions[i].comparison) {
			case ExpressionType::COMPARE_EQUAL:
				VectorOperations::Equals(l, r, intermediate);
				break;
			case ExpressionType::COMPARE_NOTEQUAL:
				VectorOperations::NotEquals(l, r, intermediate);
				break;
			case ExpressionType::COMPARE_LESSTHAN:
				VectorOperations::LessThan(l, r, intermediate);
				break;
			case ExpressionType::COMPARE_GREATERTHAN:
				VectorOperations::GreaterThan(l, r, intermediate);
				break;
			case ExpressionType::COMPARE_LESSTHANOREQUALTO:
				VectorOperations::LessThanEquals(l, r, intermediate);
				break;
			case ExpressionType::COMPARE_GREATERTHANOREQUALTO:
				VectorOperations::GreaterThanEquals(l, r, intermediate);
				break;
			default:
				throw NotImplementedException(
				    "Unsupported join comparison expression %s",
				    ExpressionTypeToString(conditions[i].comparison).c_str());
			}
			if (i == 0) {
				// first predicate, move to the final result
				intermediate.Move(final_result);
			} else {
				// subsequent predicates: AND together
				VectorOperations::And(intermediate, final_result, final_result);
			}
		}
		assert(final_result.type == TypeId::BOOLEAN);
		state->right_chunk++;
		bool is_last_chunk =
		    state->right_chunk >= state->right_chunks.chunks.size();
		// now create the final result
		bool next_chunk =
		    CreateResult(left_chunk, state->left_position, right_chunk, chunk,
		                 final_result, is_last_chunk);
		if (is_last_chunk || next_chunk) {
			// if we have exhausted all the chunks, move to the next tuple in
			// the left set
			state->left_position++;
			state->right_chunk = 0;
		}
	} while (chunk.size() == 0);
}

unique_ptr<PhysicalOperatorState>
PhysicalNestedLoopJoin::GetOperatorState(ExpressionExecutor *parent_executor) {
	return make_unique<PhysicalNestedLoopJoinOperatorState>(
	    children[0].get(), children[1].get(), parent_executor);
}
