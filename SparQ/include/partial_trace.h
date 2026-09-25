/**
 * @file partial_trace.h
 * @brief Partial trace operation definitions
 * @details Implements partial trace operations on quantum states, used for reduced density matrices
 *          and probability computation
 */

#pragma once
#include "basic_components.h"
#ifdef USE_CUDA
#include "cuda/cuda_utils.cuh"
#endif

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Partial trace operation class
	 * @details Performs the partial trace operation on the specified registers
	 */
	struct PartialTrace {
		/** @brief List of partial trace register IDs */
		std::vector<size_t> partial_trace_registers;

		/**
		 * @brief Constructor (name list version)
		 * @param partial_trace_register_names List of register names
		 */
		PartialTrace(const std::vector<std::string>& partial_trace_register_names);

		/**
		 * @brief Constructor (ID list version)
		 * @param partial_trace_register_names List of register IDs
		 */
		PartialTrace(const std::vector<size_t>& partial_trace_register_names);

		/**
		 * @brief Constructor (single name version)
		 * @param partial_trace_register_name Register name
		 */
		PartialTrace(std::string_view partial_trace_register_name);

		/**
		 * @brief Constructor (single ID version)
		 * @param partial_trace_register_name Register ID
		 */
		PartialTrace(size_t partial_trace_register_name);

		/**
		 * @brief Apply the partial trace operation
		 * @param state System state vector
		 * @return List of values and probability after the partial trace
		 */
		std::pair<std::vector<uint64_t>, double> operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the partial trace operation (sparse state version)
		 * @param state Sparse state
		 * @return List of values and probability after the partial trace
		 */
		std::pair<std::vector<uint64_t>, double> operator()(SparseState& state) const
		{
			return (*this)(state.basis_states);
		}

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the partial trace operation
		 * @param state CUDA sparse state
		 * @return List of values and probability after the partial trace
		 */
		std::pair<std::vector<uint64_t>, double> operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Selective partial trace operation class
	 * @details Performs the partial trace operation on the specified registers and selects specific values
	 */
	struct PartialTraceSelect {
		/** @brief List of partial trace register IDs */
		std::vector<size_t> partial_trace_registers;

		/** @brief List of selected values */
		std::vector<uint64_t> select_values;

		/**
		 * @brief Constructor (name-to-value map version)
		 * @param partial_traces Map from register names to values
		 */
		PartialTraceSelect(const std::map<std::string_view, uint64_t>& partial_traces);

		/**
		 * @brief Constructor (ID-to-value map version)
		 * @param partial_traces Map from register IDs to values
		 */
		PartialTraceSelect(const std::map<size_t, uint64_t>& partial_traces);

		/**
		 * @brief Constructor (name list and value list version)
		 * @param partial_trace_regs_ List of register names
		 * @param select_values_ List of selected values
		 */
		PartialTraceSelect(const std::vector<std::string>& partial_trace_regs_,
			const std::vector<uint64_t> &select_values_);

		/**
		 * @brief Constructor (ID list and value list version)
		 * @param partial_trace_regs_ List of register IDs
		 * @param select_values_ List of selected values
		 */
		PartialTraceSelect(const std::vector<size_t>& partial_trace_regs_,
			const std::vector<uint64_t> &select_values_);

		/**
		 * @brief Apply the selective partial trace operation
		 * @param state System state vector
		 * @return Probability of the selected states
		 */
		double operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the selective partial trace operation (sparse state version)
		 * @param state Sparse state
		 * @return Probability of the selected states
		 */
		double operator()(SparseState& state) const
		{
			return (*this)(state.basis_states);
		}

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the selective partial trace operation
		 * @param state CUDA sparse state
		 * @return Probability of the selected states
		 */
		double operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Range-selective partial trace operation class
	 * @details Performs the partial trace operation on the specified register and selects a range of values
	 */
	struct PartialTraceSelectRange {
		/** @brief Partial trace register ID */
		size_t partial_trace_register;

		/** @brief Selection range */
		std::pair<size_t, size_t> select_range;

		/** @brief Result probability */
		double r = 0.0;

		/**
		 * @brief Constructor (name version)
		 * @param partial_trace_register_ Register name
		 * @param select_range_ Selection range [min, max]
		 */
		PartialTraceSelectRange(std::string_view partial_trace_register_,
			std::pair<size_t, size_t> select_range_) :
			partial_trace_register(System::get(partial_trace_register_)),
			select_range(select_range_)
		{
		}

		/**
		 * @brief Constructor (ID version)
		 * @param partial_trace_register_ Register ID
		 * @param select_range_ Selection range [min, max]
		 */
		PartialTraceSelectRange(size_t partial_trace_register_,
			std::pair<size_t, size_t> select_range_) :
			partial_trace_register(partial_trace_register_),
			select_range(select_range_)
		{
		}

		/**
		 * @brief Apply the range-selective partial trace operation
		 * @param state System state vector
		 * @return Probability of the selected states
		 */
		double operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the range-selective partial trace operation (sparse state version)
		 * @param state Sparse state
		 * @return Probability of the selected states
		 */
		double operator()(SparseState& state) const
		{
			return (*this)(state.basis_states);
		}

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the range-selective partial trace operation
		 * @param state CUDA sparse state
		 * @return Probability of the selected states
		 */
		double operator()(CuSparseState& state);
#endif
	};


}
