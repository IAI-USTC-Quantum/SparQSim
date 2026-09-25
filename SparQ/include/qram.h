/**
 * @file qram.h
 * @brief QRAM (Quantum Random Access Memory) operation definitions
 * @details Implements the load operations and the input generator of the quantum random access
 *          memory, supporting QRAMLoad, QRAMLoadFast and QRAMInputGenerator
 */

#pragma once
#include "basic_components.h"
#include "dark_magic.h"
#include "sort_state.h"

namespace qram_simulator 
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief QRAM load operation class
	 * @details Implements the standard load operation of the quantum random access memory
	 * @note Only adapted for qram_qutrit::QRAMCircuit; use QRAMLoad_Qubit for the qubit version
	 */
	struct QRAMLoad : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief QRAM circuit pointer */
		const qram_qutrit::QRAMCircuit* qram;

		/** @brief Address register ID */
		size_t register_addr;

		/** @brief Data register ID */
		size_t register_data;

		/** @brief Version string */
		static std::string version;

		ClassControllable

		/**
		 * @brief Constructor (ID version)
		 * @param qram_ QRAM circuit pointer
		 * @param reg1 Address register ID
		 * @param reg2 Data register ID
		 * @throws Throws an exception when the address register type is not an unsigned integer
		 */
		QRAMLoad(const qram_qutrit::QRAMCircuit* qram_, size_t reg1, size_t reg2)
			: register_addr(reg1), register_data(reg2)
		{
			qram = qram_;
			if (qram == nullptr || register_addr == register_data)
				throw_invalid_input();

			/* Type check */
#ifndef QRAM_Release
			if (System::type_of(register_addr) != UnsignedInteger)
				throw_invalid_input();
#endif
		}

		/**
		 * @brief Constructor (name version)
		 * @param qram QRAM circuit pointer
		 * @param reg1 Address register name
		 * @param reg2 Data register name
		 */
		QRAMLoad(const qram_qutrit::QRAMCircuit* qram, std::string_view reg1, std::string_view reg2)
			: QRAMLoad(qram, System::get(reg1), System::get(reg2))
		{ }

		/**
		 * @brief Noise-free implementation
		 * @param state System state vector
		 */
		void noise_free_impl(std::vector<System>& state) const;

		/**
		 * @brief Set branches (internal implementation)
		 * @param qram QRAM circuit pointer
		 * @param state System state vector
		 * @param groups Grouping information
		 */
		void _set_branches(qram_qutrit::QRAMCircuit* qram,
			const std::vector<System>& state,
			std::vector<std::pair<size_t, size_t>>& groups) const;

		/**
		 * @brief Set branches implementation (recursive)
		 * @param qram QRAM circuit pointer
		 * @param state System state vector
		 * @param branches Branch information
		 * @param branch_probs Branch probabilities
		 * @param iter_l Left iteration boundary
		 * @param iter_r Right iteration boundary
		 * @param groups Grouping information
		 */
		void _set_branches_impl(qram_qutrit::QRAMCircuit* qram, const std::vector<System>& state,
			decltype(qram->get_branches()) branches,
			decltype(qram->get_branch_probs()) branch_probs,
			size_t iter_l, size_t iter_r,
			std::vector<std::pair<size_t, size_t>>& groups) const;

		/**
		 * @brief Reconstruct operation
		 * @param qram QRAM circuit pointer
		 * @param state System state vector
		 * @param groups Grouping information
		 */
		void _reconstruct(qram_qutrit::QRAMCircuit* qram, 
			std::vector<System>& state,
			std::vector<std::pair<size_t, size_t>>& groups) const;

		/**
		 * @brief Apply the QRAM load operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA noise-free implementation
		 * @param state CUDA sparse state
		 */
		void noise_free_impl(CuSparseState& state) const;

		/**
		 * @brief CUDA apply the QRAM load operation
		 * @param state CUDA sparse state
		 */
		void operator()(CuSparseState& state) const;
#endif	
	};

	/**
	 * @brief Fast QRAM load operation class
	 * @details Optimized version of the QRAM load operation with higher performance
	 * @note Only adapted for qram_qutrit::QRAMCircuit
	 */
	struct QRAMLoadFast : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief QRAM circuit pointer */
		const qram_qutrit::QRAMCircuit* qram;

		/** @brief Address register ID */
		size_t register_addr;

		/** @brief Data register ID */
		size_t register_data;

		ClassControllable

		/**
		 * @brief Constructor (ID version)
		 * @param qram QRAM circuit pointer
		 * @param reg1 Address register ID
		 * @param reg2 Data register ID
		 */
		QRAMLoadFast(const qram_qutrit::QRAMCircuit* qram, size_t reg1, size_t reg2);

		/**
		 * @brief Constructor (name version)
		 * @param qram QRAM circuit pointer
		 * @param reg1 Address register name
		 * @param reg2 Data register name
		 */
		QRAMLoadFast(const qram_qutrit::QRAMCircuit* qram, std::string_view reg1, std::string_view reg2);

		/**
		 * @brief Noise-free implementation
		 * @param state System state vector
		 */
		void noise_free_impl(std::vector<System>& state) const;

		/**
		 * @brief Implementation with damping
		 * @param state System state vector
		 * @param qram QRAM circuit pointer
		 * @param state_remove_cache State removal cache
		 */
		void has_damping_impl(std::vector<System>& state, qram_qutrit::QRAMCircuit* qram, std::vector<System>& state_remove_cache) const;

		/**
		 * @brief Implementation without damping
		 * @param state System state vector
		 * @param qram QRAM circuit pointer
		 * @param state_remove_cache State removal cache
		 */
		void no_damping_impl(std::vector<System>& state, qram_qutrit::QRAMCircuit* qram, std::vector<System>& state_remove_cache) const;

		/**
		 * @brief Apply the fast QRAM load operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;
	};


	/**
	 * @brief QRAM input generator
	 * @details Generates random input states for QRAM testing
	 */
	struct QRAMInputGenerator
	{
		/** @brief Unique input set */
		std::set<std::pair<size_t, size_t>> unique_set;

		/** @brief Address size */
		size_t addr_sz;

		/** @brief Data size */
		size_t data_sz;

		/** @brief Input size */
		size_t input_sz;

		/** @brief Address distribution */
		std::uniform_int_distribution<size_t> addr_dist;

		/** @brief Data distribution */
		std::uniform_int_distribution<size_t> data_dist;

		/** @brief Address value */
		size_t addr;

		/** @brief Data value */
		size_t data;

		/**
		 * @brief Constructor (random address and data)
		 * @param addr_sz_ Address size
		 * @param data_sz_ Data size
		 * @param input_size_ Input size
		 */
		QRAMInputGenerator(size_t addr_sz_, size_t data_sz_, size_t input_size_)
			: addr_sz(addr_sz_), data_sz(data_sz_), input_sz(input_size_),
			addr(std::numeric_limits<size_t>::max()), data(std::numeric_limits<size_t>::max()),
			addr_dist(0, pow2(addr_sz) - 1), data_dist(0, pow2(data_sz) - 1)
		{
			if (input_sz > pow2(addr_sz + data_sz))
			{
				input_sz = pow2(addr_sz + data_sz);
			}
		}

		/**
		 * @brief Constructor (specified address and data)
		 * @param addr_sz_ Address size
		 * @param data_sz_ Data size
		 * @param input_size_ Input size
		 * @param addr_ Specified address
		 * @param data_ Specified data
		 */
		QRAMInputGenerator(size_t addr_sz_, size_t data_sz_, size_t input_size_, size_t addr_, size_t data_)
			: addr_sz(addr_sz_), data_sz(data_sz_), input_sz(input_size_),
			addr(addr_), data(data_),
			addr_dist(0, pow2(addr_sz) - 1), data_dist(0, pow2(data_sz) - 1)
		{
			if (input_sz > pow2(addr_sz + data_sz))
			{
				input_sz = pow2(addr_sz + data_sz);
			}
		}

		/**
		 * @brief Generate a random input
		 * @return Random pair of address and data
		 */
		std::pair<size_t, size_t> rand_input()
		{
			return {
				addr_dist(random_engine::get_engine()),
				data_dist(random_engine::get_engine())
			};
		}

		/**
		 * @brief Validate registers (internal use)
		 * @param addr_ Address register ID
		 * @param data_ Data register ID
		 * @throws Throws an exception when a register ID is out of range
		 */
		void _validate_registers(size_t addr_, size_t data_) const
		{
			if (addr_ >= System::name_register_map.size() ||
				data_ >= System::name_register_map.size() ||
				!System::status_of(addr_) ||
				!System::status_of(data_))
			{
				throw_invalid_input();
			}
		}

		/**
		 * @brief Generate input state (with specified registers)
		 * @param s System state vector
		 * @param addr_ Address register ID
		 * @param data_ Data register ID
		 */
		void generate_input(std::vector<System>& s, size_t addr_, size_t data_)
		{
			s.clear();

			if (input_sz == pow2(addr_sz + data_sz))
			{
				generate_full_input(s, addr_, data_);
				return;
			}

			unique_set.clear();
			for (size_t i = 0; i < input_sz; )
			{
				auto&& input = rand_input();
				auto&& res = unique_set.insert(input);
				if (res.second)
				{
					i++;
					s.emplace_back();
					s.back().get(addr_).value = input.first;
					s.back().get(data_).value = input.second;
				}
			}
			SortUnconditional()(s);
			Normalize()(s);
		}

		/**
		 * @brief Generate input state (using preset registers)
		 * @param s System state vector
		 */
		void generate_input(std::vector<System>& s)
		{
			_validate_registers(addr, data);
			generate_input(s, addr, data);
		}

		/**
		 * @brief Generate the full input (all possible address-data combinations)
		 * @param s System state vector
		 * @param addr_ Address register ID
		 * @param data_ Data register ID
		 */
		void generate_full_input(std::vector<System>& s, size_t addr_, size_t data_)
		{
			s.clear();
			double amplitude = 1.0 / std::sqrt(input_sz);
			for (size_t i = 0; i < pow2(addr_sz); ++i)
			{
				for (size_t j = 0; j < pow2(data_sz); ++j)
				{
					s.emplace_back();
					s.back().get(addr_).value = i;
					s.back().get(data_).value = j;
					s.back().amplitude = amplitude;

					unique_set.insert({ i,j });
				}
			}
		}
	};
}
