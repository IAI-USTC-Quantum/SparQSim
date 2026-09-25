/**
 * @file quantum_interfere_basic.h
 * @brief Quantum interference basic utility definitions
 * @details Provides basic utility classes such as state hashing, equality comparison and less-than comparison,
 *          used for state management and lookup in quantum interference operations
 */

#pragma once
#include "system_operations.h"
#include "sort_state.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief State hash excluding a specified key
	 * @details Excludes the specified register when computing the state hash value
	 */
	struct StateHashExceptKey {
		/** @brief Excluded key (register ID) */
		size_t id;

		/**
		 * @brief Constructor
		 * @param id_ ID of the excluded register
		 */
		StateHashExceptKey(size_t id_)
			: id(id_)
		{}

		/**
		 * @brief Compute the hash value
		 * @param v System state
		 * @return Hash value
		 */
		size_t operator()(const System& v) const;
	};

	/**
	 * @brief State hash excluding specified qubits
	 * @details Excludes the specified qubits of a register when computing the state hash value
	 */
	struct StateHashExceptQubits {
		/** @brief Register ID */
		size_t id;

		/** @brief Set of qubit positions */
		std::set<size_t> qubit_positions;

		/**
		 * @brief Constructor
		 * @param id_ Register ID
		 * @param qubit_positions_ Set of qubit positions
		 */
		StateHashExceptQubits(size_t id_, std::set<size_t> qubit_positions_)
			: id(id_), qubit_positions(qubit_positions_)
		{}

	/**
	 * @brief Compute the hash value
	 * @param v System state
	 * @return Hash value
	 */
		size_t operator()(const System& v) const;
	};

	/**
	 * @brief State equality comparison excluding a specified key
	 * @details Excludes the specified register when comparing two states for equality
	 */
	struct StateEqualExceptKey {
		/** @brief Excluded key (register ID) */
		size_t id;

		/**
		 * @brief Constructor
		 * @param id_ ID of the excluded register
		 */
		StateEqualExceptKey(size_t id_) : id(id_) {}

		/**
		 * @brief Equality comparison
		 * @param v1 First system state
		 * @param v2 Second system state
		 * @return Whether they are equal
		 */
		size_t operator()(const System& v1, const System& v2) const;
	};

	/**
	 * @brief State equality comparison excluding specified qubits
	 * @details Excludes the specified qubits of a register when comparing two states for equality
	 */
	struct StateEqualExceptQubits {
		/** @brief Register ID */
		size_t id;

		/** @brief Set of qubit positions */
		std::set<size_t> qubit_positions;

		/**
		 * @brief Constructor
		 * @param id_ Register ID
		 * @param qubit_positions_ Set of qubit positions
		 */
		StateEqualExceptQubits(size_t id_, std::set<size_t> qubit_positions_) : id(id_), qubit_positions(qubit_positions_) {}

		/**
		 * @brief Equality comparison
		 * @param v1 First system state
		 * @param v2 Second system state
		 * @return Whether they are equal
		 */
		size_t operator()(const System& v1, const System& v2) const;
	};

	/**
	 * @brief State less-than comparison excluding a specified key
	 * @details Excludes the specified register when comparing the order of two states
	 */
	struct StateLessExceptKey {
		/** @brief Excluded key (register ID) */
		size_t id;

		/**
		 * @brief Constructor
		 * @param id_ ID of the excluded register
		 */
		StateLessExceptKey(size_t id_) : id(id_) {}

		/**
		 * @brief Less-than comparison
		 * @param v1 First system state
		 * @param v2 Second system state
		 * @return Whether v1 is less than v2
		 */
		size_t operator()(const System& v1, const System& v2) const;
	};

	/**
	 * @brief State less-than comparison excluding specified qubits
	 * @details Excludes the specified qubits of a register when comparing the order of two states
	 */
	struct StateLessExceptQubits {
		/** @brief Register ID */
		size_t id;

		/** @brief Bit mask */
		size_t mask;

		/** @brief Set of qubit IDs */
		std::set<size_t> qubit_ids;

		/**
		 * @brief Constructor
		 * @param id_ Register ID
		 * @param qubit_ids_ Set of qubit IDs
		 */
		StateLessExceptQubits(size_t id_, std::set<size_t> qubit_ids_) : id(id_), qubit_ids(qubit_ids_)
		{
			mask = make_mask(qubit_ids);
		}

		/**
		 * @brief Remove the specified bit
		 * @param val Original value
		 * @return Value with the bit removed
		 */
		inline size_t remove_digits(size_t val) const
		{
			return val & mask;
		}

		/**
		 * @brief Create a bit mask
		 * @param qubit_ids Set of qubit IDs
		 * @return Bit mask
		 */
		inline size_t make_mask(const std::set<size_t>& qubit_ids)
		{
			size_t mask = 0;
			for (auto id : qubit_ids)
			{
				mask += pow2(id);
			}
			mask = ~mask;
			return mask;
		}

		/**
		 * @brief Less-than comparison
		 * @param v1 First system state
		 * @param v2 Second system state
		 * @return Whether v1 is less than v2
		 */
		size_t operator()(const System& v1, const System& v2) const;
	};

}
