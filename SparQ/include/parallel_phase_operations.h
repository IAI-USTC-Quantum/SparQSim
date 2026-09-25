/**
 * @file parallel_phase_operations.h
 * @brief Parallel phase operation definitions
 * @details Implements parallel conditional phase flips and global phase operations,
 *          supporting zero-conditional phase flips, range-conditional phase flips and reflection operations
 */

#pragma once
#include "basic_components.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Zero-conditional phase flip
	 * @details Applies a phase flip when the specified registers are all zero
	 */
	struct ZeroConditionalPhaseFlip : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief List of register IDs */
		std::vector<size_t> ids;

		ClassControllable

		/**
		 * @brief Constructor (ID list version)
		 * @param regs_ List of register IDs
		 */
		ZeroConditionalPhaseFlip(const std::vector<size_t> &regs_)
			: ids(regs_)
		{
		}

		/**
		 * @brief Constructor (name list version)
		 * @param regs_ List of register names
		 */
		ZeroConditionalPhaseFlip(const std::vector<std::string> &regs_)
		{
			ids.reserve(regs_.size());
			for (const auto& reg : regs_)
			{
				ids.push_back(System::get(reg));
			}
		}

		/**
		 * @brief Apply the phase flip operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the phase flip operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Range-conditional phase flip
	 * @details Applies a phase flip when the register value is within the specified range
	 */
	struct RangeConditionalPhaseFlip : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Value range */
		size_t value_range;

		/**
		 * @brief Constructor (ID version)
		 * @param id_ Register ID
		 * @param range_ Value range
		 */
		RangeConditionalPhaseFlip(size_t id_, size_t range_)
			:id(id_), value_range(range_)
		{}

		/**
		 * @brief Constructor (name version)
		 * @param reg_ Register name
		 * @param range_ Value range
		 */
		RangeConditionalPhaseFlip(std::string_view reg_, size_t range_)
			:id(System::get(reg_)), value_range(range_)
		{}

		/**
		 * @brief Apply the phase flip operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;
	};

	/**
	 * @brief Boolean reflection operation
	 * @details Implements the Grover reflection operation: (I - 2|0><0|) or (2|0><0| - I)
	 */
	struct Reflection_Bool : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief List of register IDs */
		std::vector<size_t> regs;

		/** @brief Whether to use the inverse form (true: I-2|0><0|, false: 2|0><0|-I) */
		bool inverse;

		ClassControllable

		/**
		 * @brief Constructor (single register name version)
		 * @param reg_ Register name
		 * @param inverse_ Whether to use the inverse form (default false)
		 */
		Reflection_Bool(std::string_view reg_, bool inverse_ = false)
			: inverse(inverse_)
		{
			regs.push_back(System::get(reg_));
		}

		/**
		 * @brief Constructor (single register ID version)
		 * @param id_ Register ID
		 * @param inverse_ Whether to use the inverse form (default false)
		 */
		Reflection_Bool(size_t id_, bool inverse_ = false)
			: inverse(inverse_)
		{
			regs.push_back(id_);
		}

		/**
		 * @brief Constructor (multiple register names version)
		 * @param regs_ List of register names
		 * @param inverse_ Whether to use the inverse form (default false)
		 */
		Reflection_Bool(const std::vector<std::string> &regs_, bool inverse_ = false)
			: inverse(inverse_)
		{
			for (auto& reg : regs_) regs.push_back(System::get(reg));
		}

		/**
		 * @brief Constructor (multiple register IDs version)
		 * @param ids_ List of register IDs
		 * @param inverse_ Whether to use the inverse form (default false)
		 */
		Reflection_Bool(const std::vector<size_t> &ids_, bool inverse_ = false)
			: inverse(inverse_)
		{
			regs = ids_;
		}

		/**
		 * @brief Apply the reflection operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the reflection operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Global phase operation (integer register)
	 * @details Applies a global phase to the whole state
	 */
	struct GlobalPhase : BaseOperator
	{
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Phase factor */
		complex_t c;

		ClassControllable

		/**
		 * @brief Constructor
		 * @param c_ Phase factor
		 */
		GlobalPhase(complex_t c_) : c(c_) {};

		/**
		 * @brief Apply the global phase operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation (conjugate phase)
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the global phase operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA apply the dagger operation
		 * @param state CUDA sparse state
		 */
		void dag(CuSparseState& state) const;
#endif
	};

	// Deprecated alias; see docs/naming_conventions.md. Remove in the next major version.
	using GlobalPhase_Int [[deprecated("use GlobalPhase")]] = GlobalPhase;
}
