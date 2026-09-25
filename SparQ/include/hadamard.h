/**
 * @file hadamard.h
 * @brief Hadamard gate operation definitions
 * @details Implements several variants of the Hadamard gate, including integer-register Hadamard,
 *          full Hadamard, single-bit Hadamard, and partial-qubit Hadamard
 */

#pragma once
#include "quantum_interfere_basic.h"


namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief Integer Hadamard gate
	 * @details Applies the Hadamard transform to an entire integer register, turning computational
	 *          basis states into superpositions
	 */
	struct Hadamard_Int : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Bit mask */
		uint64_t mask;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 * @param n_digits_ Number of qubits
		 */
		Hadamard_Int(std::string_view reg_in, size_t n_digits_)
			: Hadamard_Int(System::get(reg_in), n_digits_)
		{
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 * @param n_digits_ Number of qubits
		 */
		Hadamard_Int(size_t reg_in, size_t n_digits_)
		{
			id = reg_in;
			n_digits = n_digits_;
			mask = pow2(n_digits);
			mask--;
			mask = ~mask;
		}

		/**
		 * @brief Get the value at the given position (helper function)
		 * @param i Index
		 * @param state System state vector
		 * @return Reference to the value
		 */
		inline size_t& val(size_t i, std::vector<System>& state) const
		{
			return state[i].get(id).value;
		}

		/**
		 * @brief Perform the operation over the given range
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 */
		void operate(size_t l, size_t r, std::vector<System>& state) const;

		/**
		 * @brief Apply the Hadamard gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const; 

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the Hadamard gate operation
		 * @param s CUDA sparse state
		 */
		void operator()(CuSparseState& s) const;
#endif
	};

	/**
	 * @brief Full Hadamard gate
	 * @details Applies the full Hadamard transform to an entire register, containing all possible output states
	 */
	struct Hadamard_Int_Full : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Threshold */
		const size_t few_threshold = n_digits - 1;

		/** @brief Full state size */
		size_t full_size;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 */
		Hadamard_Int_Full(std::string_view reg_in)
			: id(System::get(reg_in)), n_digits(System::size_of(reg_in)),
			full_size(pow2(n_digits)) {}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 */
		Hadamard_Int_Full(size_t reg_in)
			: id(reg_in), n_digits(System::size_of(reg_in)),
			full_size(pow2(n_digits)) {}

		/**
		 * @brief Get the value at the given position (helper function)
		 * @param i Index
		 * @param state System state vector
		 * @return Reference to the value
		 */
		inline size_t& val(size_t i, std::vector<System>& state) const
		{
			return state[i].get(id).value;
		}

		/**
		 * @brief Apply the Hadamard gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Sparse bucket operation
		 * @param positions List of positions
		 * @param state System state vector
		 */
		void operate_bucket_sparse(const std::vector<size_t>& positions, std::vector<System>& state) const;

		/**
		 * @brief In-place bucket operation
		 * @param positions List of positions
		 * @param state System state vector
		 */
		void operate_bucket_inplace(const std::vector<size_t>& positions, std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the Hadamard gate operation
		 * @param s CUDA sparse state
		 */
		void operator()(CuSparseState& s) const;
#endif
	};

	/**
	 * @brief Single-bit Hadamard gate
	 * @details A Hadamard gate applicable only to single-bit registers
	 */
	struct Hadamard_Bool : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Output register ID */
		size_t out_id;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 * @throws Throws an exception when the register size is not 1
		 */
		Hadamard_Bool(std::string_view reg_in)
			: out_id(System::get(reg_in))
		{
			if (System::size_of(out_id) != 1)
				throw_invalid_input("Hadamard_Bool: size of output register must be 1");
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 * @throws Throws an exception when the register size is not 1
		 */
		Hadamard_Bool(size_t reg_in)
			: out_id(reg_in)
		{
			if (System::size_of(out_id) != 1)
				throw_invalid_input("Hadamard_Bool: size of output register must be 1");
		}

		/**
		 * @brief Paired operation (V2 version)
		 * @param zero |0> branch index
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_pair(size_t zero, size_t one, std::vector<System>& state) const;

		/**
		 * @brief Operate on the |0> branch alone
		 * @param zero |0> branch index
		 * @param state System state vector
		 */
		void operate_alone_zero(size_t zero, std::vector<System>& state) const;

		/**
		 * @brief Operate on the |1> branch alone
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_alone_one(size_t one, std::vector<System>& state) const;

		/**
		 * @brief Apply the Hadamard gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the Hadamard gate operation
		 * @param s CUDA sparse state
		 */
		void operator()(CuSparseState& s) const;
#endif
	};

	/**
	 * @brief Partial-qubit Hadamard gate
	 * @details Applies the Hadamard transform only to a subset of the qubits in a register
	 */
	struct Hadamard_Partial : SelfAdjointOperator {
		using SelfAdjointOperator::operator();
		using SelfAdjointOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Bit mask */
		size_t mask;

		/** @brief Set of qubit positions */
		std::set<size_t> qubit_positions;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 * @param qubit_positions_ Set of qubit positions
		 */
		Hadamard_Partial(std::string_view reg_in, std::set<size_t>& qubit_positions_)
			: id(System::get(reg_in)), qubit_positions(qubit_positions_)
		{
			mask = make_mask(qubit_positions_);
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 * @param qubit_positions_ Set of qubit positions
		 */
		Hadamard_Partial(size_t reg_in, std::set<size_t>& qubit_positions_)
			: id(reg_in), qubit_positions(qubit_positions_)
		{
			mask = make_mask(qubit_positions_);
		}

		/**
		 * @brief Get the value at the given position (helper function)
		 * @param i Index
		 * @param state System state vector
		 * @return Reference to the value
		 */
		inline size_t& val(size_t i, std::vector<System>& state) const
		{
			return state[i].get(id).value;
		}

		/**
		 * @brief Paired operation
		 * @param zero |0> branch index
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_pair(size_t zero, size_t one, std::vector<System>& state) const;

		/**
		 * @brief Operate on the |0> branch alone
		 * @param zero |0> branch index
		 * @param state System state vector
		 */
		void operate_alone_zero(size_t zero, std::vector<System>& state) const;

		/**
		 * @brief Operate on the |1> branch alone
		 * @param one |1> branch index
		 * @param state System state vector
		 */
		void operate_alone_one(size_t one, std::vector<System>& state) const;

		/**
		 * @brief Perform the operation over the given range
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 */
		void operate(size_t l, size_t r, std::vector<System>& state) const;

		/**
		 * @brief Apply the partial Hadamard gate operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;
	};

	// Deprecated alias; see docs/naming_conventions.md. Remove in the next major version.
	using Hadamard_PartialQubit [[deprecated("use Hadamard_Partial")]] = Hadamard_Partial;
}
