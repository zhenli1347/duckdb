//===----------------------------------------------------------------------===//
//                         DuckDB
//
// duckdb/parallel/pipeline.hpp
//
//
//===----------------------------------------------------------------------===//

#pragma once

#include "duckdb/common/unordered_set.hpp"
#include "duckdb/execution/physical_operator.hpp"
#include "duckdb/function/table_function.hpp"
#include "duckdb/parallel/parallel_state.hpp"
#include "duckdb/parallel/task_scheduler.hpp"
#include "duckdb/common/atomic.hpp"

namespace duckdb {
class Executor;
class Event;

//! The Pipeline class represents an execution pipeline
class Pipeline : public std::enable_shared_from_this<Pipeline> {
	friend class Executor;
	friend class PipelineExecutor;
	friend class PipelineEvent;

public:
	Pipeline(Executor &execution_context);

	Executor &executor;

public:
	ClientContext &GetClientContext();

	void AddDependency(shared_ptr<Pipeline> &pipeline);

	void Ready();
	void Reset();
	void ResetSource();
	void Schedule(shared_ptr<Event> &event);

	//! Finalize this pipeline
	void Finalize(Event &event);

	string ToString() const;
	void Print() const;

	//! Returns query progress
	bool GetProgress(int &current_percentage);

	//! Returns a list of all operators (including source and sink) involved in this pipeline
	vector<PhysicalOperator *> GetOperators() const;
private:
	//! Whether or not the pipeline has been readied
	bool ready;
	//! The source of this pipeline
	PhysicalOperator *source;
	//! The chain of intermediate operators
	vector<PhysicalOperator *> operators;
	//! The sink (i.e. destination) for data; this is e.g. a hash table to-be-built
	PhysicalOperator *sink;

	//! The global source state
	unique_ptr<GlobalSourceState> source_state;

	//! The parent pipelines (i.e. pipelines that are dependent on this pipeline to finish)
	vector<weak_ptr<Pipeline>> parents;
	//! The dependencies of this pipeline
	vector<weak_ptr<Pipeline>> dependencies;
	//! The adjacent union pipelines of this pipeline
	//! Union pipelines have the same sink, but can be run concurrently along with this pipeline
	vector<shared_ptr<Pipeline>> union_pipelines;
	//! Child pipelines of this pipeline
	//! Child pipelines should be run AFTER this pipeline is completed, but share the same sink
	//! i.e. they should be run after this pipeline is completed, but before the finalize is called
	vector<shared_ptr<Pipeline>> child_pipelines;

private:
	bool GetProgressInternal(ClientContext &context, PhysicalOperator *op, int &current_percentage);
	void ScheduleSequentialTask(shared_ptr<Event> &event);
	bool LaunchScanTasks(shared_ptr<Event> &event, idx_t max_threads);

	bool ScheduleParallel(shared_ptr<Event> &event);

	//! Add a pipeline to this pipeline with the same sink
	void AddUnionPipeline(shared_ptr<Pipeline> pipeline);
};

} // namespace duckdb
