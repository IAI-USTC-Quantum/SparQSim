/**
 * @file rot.h
 * @brief Rotation gate and state preparation operation definitions
 * @details Implements general unitary rotation gates and quantum state preparation operations,
 *          supporting unitary matrix rotations of arbitrary dimension and Schmidt-decomposition state preparation
 */

#pragma once
#include "quantum_interfere_basic.h"
#include "matrix.h"

namespace qram_simulator
{
	/** @namespace qram_simulator
	 * @brief QRAM sparse state simulator namespace
	 */

	/**
	 * @brief General unitary rotation gate
	 * @details Applies a unitary matrix of arbitrary dimension to an integer register
	 */
	struct Rot_GeneralUnitary : BaseOperator
	{
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Unitary matrix type */
		using unitary_t = DenseMatrix<complex_t>;

		/** @brief Unitary matrix */
		unitary_t mat;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Full state size */
		size_t full_size;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 * @param mat_ Unitary matrix
		 * @throws Throws an exception when the matrix size does not match the register size
		 */
		Rot_GeneralUnitary(std::string_view reg_in, const unitary_t &mat_)
			: id(System::get(reg_in)), mat(mat_)
		{
			n_digits = System::size_of(id);
			full_size = pow2(n_digits);

			if (full_size != mat_.size)
				throw_invalid_input("Matrix size does not match the register's size.");
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 * @param mat_ Unitary matrix
		 * @throws Throws an exception when the matrix size does not match the register size
		 */
		Rot_GeneralUnitary(size_t reg_in, const unitary_t &mat_)
			: id(reg_in), mat(mat_)
		{
			n_digits = System::size_of(id);
			full_size = pow2(n_digits);

			if (full_size != mat_.size)
				throw_invalid_input("Matrix size does not match the register's size.");
		}

		/**
		 * @brief In-place bucket operation
		 * @param positions Position list
		 * @param state System state vector
		 * @param dagger Whether this is the dagger operation
		 */
		void operate_bucket_inplace(const std::vector<size_t>& positions, std::vector<System>& state, bool dagger) const;

		/**
		 * @brief Perform the rotation operation
		 * @param state System state vector
		 * @param dagger Whether this is the dagger operation
		 */
		void operate(std::vector<System>& state, bool dagger) const;

		/**
		 * @brief Apply the rotation operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the rotation operation
		 * @param state CUDA sparse state
		 */
		virtual void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA apply the dagger operation
		 * @param state CUDA sparse state
		 */
		virtual void dag(CuSparseState& state) const;
#endif
	};

	/**
	 * @brief Build a state preparation unitary using Schmidt decomposition
	 * @param vec Target state vector
	 * @return State preparation unitary matrix
	 */
	DenseMatrix<complex_t> stateprep_unitary_build_schmidt(const std::vector<complex_t>& vec);

	/**
	 * @brief General state preparation operation
	 * @details Uses a unitary matrix to prepare |0...0> into the target quantum state
	 */
	struct Rot_GeneralStatePrep : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Unitary matrix type */
		using unitary_t = DenseMatrix<complex_t>;

		/** @brief Target state vector */
		std::vector<complex_t> vec;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Full state size */
		size_t full_size;

		/** @brief General rotation gate */
		Rot_GeneralUnitary rot_general;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 * @param vec Target state vector
		 */
		Rot_GeneralStatePrep(std::string_view reg_in, const std::vector<complex_t> &vec);

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 * @param vec Target state vector
		 */
		Rot_GeneralStatePrep(size_t reg_in, const std::vector<complex_t> &vec);

		/**
		 * @brief Apply the state preparation operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

#ifdef USE_CUDA
		/**
		 * @brief CUDA apply the state preparation operation
		 * @param state CUDA sparse state
		 */
		virtual void operator()(CuSparseState& state) const;

		/**
		 * @brief CUDA apply the dagger operation
		 * @param state CUDA sparse state
		 */
		virtual void dag(CuSparseState& state) const;
#endif
	};
}
