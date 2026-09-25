/**
 * @file sort_state.h
 * @brief State sorting definitions
 * @details Implements various sorting operations on quantum states, supporting sorting by key,
 *          unconditional sorting, sorting by amplitude, and other sorting modes
 */

#pragma once
#include "basic_components.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Sort excluding a key
	 * @details Excludes the specified key during sorting, so that it is moved to the end
	 */
	struct SortExceptKey : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Excluded key (register ID) */
		size_t id;

		/**
		 * @brief Constructor (name version)
		 * @param key_ Register name
		 */
		SortExceptKey(std::string_view key_)
			: id(System::get(key_))
		{}

		/**
		 * @brief Constructor (ID version)
		 * @param key_ Register ID
		 */
		SortExceptKey(size_t key_)
			: id(key_)
		{}

		/**
		 * @brief Apply the sorting operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;
	};

	/**
	 * @brief Sort by key
	 * @details Sorts by the value of the specified register
	 */
	struct SortByKey : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Sort key (register ID) */
		size_t register_key;

		/**
		 * @brief Constructor (name version)
		 * @param key Register name
		 */
		SortByKey(std::string_view key);

		/**
		 * @brief Constructor (ID version)
		 * @param key Register ID
		 */
		SortByKey(size_t key);

		/**
		 * @brief Apply the sorting operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;
	};

	/**
	 * @brief Sort excluding a bit
	 * @details Excludes the specified bit of a register during sorting
	 */
	struct SortExceptBit : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Bit index */
		size_t digit;

		/**
		 * @brief Constructor (name version)
		 * @param key_ Register name
		 * @param digit_ Bit index
		 */
		SortExceptBit(std::string_view key_, size_t digit_)
			: id(System::get(key_)), digit(digit_)
		{}

		/**
		 * @brief Constructor (ID version)
		 * @param key_ Register ID
		 * @param digit_ Bit index
		 */
		SortExceptBit(size_t key_, size_t digit_)
			: id(key_), digit(digit_)
		{}

		/**
		 * @brief Apply the sorting operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;
	};

	/**
	 * @brief Create a bit mask
	 * @param qubit_ids Set of qubit IDs
	 * @return Bit mask
	 */
	inline uint64_t make_mask(const std::set<size_t>& qubit_ids)
	{
		uint64_t mask = 0;
		for (auto id : qubit_ids)
		{
			mask += pow2(id);
		}
		mask = ~mask;
		return mask;
	}

	/**
	 * @brief Hadamard sort-except-key
	 * @details Excluded-key sorting optimized for the Hadamard operation
	 */
	struct SortExceptKeyHadamard : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Excluded key (register ID) */
		size_t id;

		/** @brief Bit mask */
		uint64_t mask;

		/** @brief Set of qubit IDs */
		std::set<size_t> qubit_ids;

		/**
		 * @brief Constructor (name version)
		 * @param key_ Register name
		 * @param qubit_ids_ Set of qubit IDs
		 */
		SortExceptKeyHadamard(std::string_view key_, std::set<size_t> qubit_ids_)
			: id(System::get(key_)), qubit_ids(qubit_ids_)
		{
			mask = make_mask(qubit_ids);
		}

		/**
		 * @brief Constructor (ID version)
		 * @param key_ Register ID
		 * @param qubit_ids_ Set of qubit IDs
		 */
		SortExceptKeyHadamard(size_t key_, std::set<size_t> qubit_ids_)
			: id(key_), qubit_ids(qubit_ids_)
		{
			mask = make_mask(qubit_ids);
		}

		/**
		 * @brief Remove the specified bit
		 * @param val Original value
		 * @return Value with the bit removed
		 */
		size_t remove_digits(size_t val) const;

		/**
		 * @brief Apply the sorting operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;
	};

	/**
	 * @brief Unconditional sorting
	 * @details Performs unconditional parallel sorting of the system states
	 */
	struct SortUnconditional : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Default constructor
		 */
		SortUnconditional() {}

		/**
		 * @brief Apply the sorting operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;
	};

	/**
	 * @brief Sort by amplitude
	 * @details Sorts the system states by amplitude
	 */
	struct SortByAmplitude : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/**
		 * @brief Default constructor
		 */
		SortByAmplitude() {}

		/**
		 * @brief Apply the sorting operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;
	};


	/**
	 * @brief Two-key sorting
	 * @details Sorts by the values of two registers
	 */
	struct SortByKey2 : SelfAdjointOperator
	{
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief First key (register ID) */
		size_t id1;

		/** @brief Second key (register ID) */
		size_t id2;

		/**
		 * @brief Constructor (name version)
		 * @param key1_ Name of the first register
		 * @param key2_ Name of the second register
		 */
		SortByKey2(std::string_view key1_, std::string_view key2_)
			: id1(System::get(key1_)), id2(System::get(key2_))
		{}

		/**
		 * @brief Constructor (ID version)
		 * @param key1_ ID of the first register
		 * @param key2_ ID of the second register
		 */
		SortByKey2(size_t key1_, size_t key2_)
			: id1(key1_), id2(key2_)
		{}

		/**
		 * @brief Apply the sorting operation
		 * @param states System state vector
		 */
		void operator()(std::vector<System>& states) const;
	};

	/**
	 * @brief Compare two systems for equality (excluding a specified key)
	 * @details Used by CondRot_General_Bool, CondRot_General_Bool_QW, Hadamard_Int, etc.
	 * @param a First system
	 * @param b Second system
	 * @param out_id Excluded key (register ID)
	 * @return Whether they are equal
	 */
	bool compare_equal(const System& a, const System& b, size_t out_id);

	/**
	 * @brief Compare two systems for equality (excluding two specified keys)
	 * @details Used by QRAM::set_branches
	 * @param a First system
	 * @param b Second system
	 * @param out_id1 First excluded key (register ID)
	 * @param out_id2 Second excluded key (register ID)
	 * @return Whether they are equal
	 */
	bool compare_equal2(const System& a, const System& b, size_t out_id1, size_t out_id2);

	/**
	 * @brief Compare two systems for equality (excluding a specified key and masked bits)
	 * @param a First system
	 * @param b Second system
	 * @param out_id Excluded key (register ID)
	 * @param mask Bit mask
	 * @return Whether they are equal
	 */
	bool compare_equal_rot(const System& a, const System& b, size_t out_id, uint64_t mask);

	/**
	 * @brief Compare two systems for equality (Hadamard version)
	 * @details Used by Hadamard_Partial; excludes the target qubit and the masked bits when comparing
	 * @param a First system
	 * @param b Second system
	 * @param out_id Excluded key (register ID)
	 * @param mask Bit mask
	 * @return Whether they are equal
	 */
	bool compare_equal_hadamard(const System& a, const System& b, size_t out_id, uint64_t mask);

}
