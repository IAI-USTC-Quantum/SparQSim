/**
 * @file dark_magic.h
 * @brief Internal optimized operations definitions
 * @details Contains low-level optimized operations such as normalization and unsafe initialization;
 *          these operations are usually used for internal implementations, use them with care
 */

#pragma once
#include "basic_components.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Normalization operation
	 * @details Normalizes a quantum state so that the total probability is 1
	 */
	struct Normalize : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Apply the normalization operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the normalization operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Unsafe initialization operation
	 * @details Directly sets a register to the specified value without safety checks
	 * @warning This operation does not check whether the value exceeds the register range; use it with care
	 */
	struct Init_Unsafe : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Value to set */
		size_t value;

		/** @brief Register ID */
		size_t id;

		/**
		 * @brief Constructor (ID version)
		 * @param id_ Register ID
		 * @param value_ Value to set
		 */
		Init_Unsafe(int id_, size_t value_) :
			value(value_), id(id_)
		{ }

		/**
		 * @brief Constructor (name version)
		 * @param reg Register name
		 * @param value_ Value to set
		 */
		Init_Unsafe(std::string_view reg, size_t value_) :
			value(value_), id(System::get(reg))
		{ }

		/**
		 * @brief Apply the unsafe initialization operation
		 * @param system_states System state vector
		 */
		void operator()(std::vector<System>& system_states) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the unsafe initialization operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif
	};
}
