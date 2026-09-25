/**
 * @file qft.h
 * @brief Quantum Fourier transform (QFT) definitions
 * @details Implements the quantum Fourier transform and its inverse, supporting the standard and full versions
 */

#pragma once
#include "quantum_interfere_basic.h"

namespace qram_simulator {

	/**
	 * @brief Quantum Fourier transform (QFT)
	 * @details Performs the quantum Fourier transform on an integer register
	 */
	struct QFT : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Root of unity omega = e^(2πi/2^n) */
		complex_t omega;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ins Register name
		 */
		QFT(std::string_view reg_ins);

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 */
		QFT(size_t reg_in);

		/**
		 * @brief Get the value at the specified position (helper function)
		 * @param i Index
		 * @param state System state vector
		 * @return Reference to the value
		 */
		inline size_t& val(size_t i, std::vector<System>& state) const
		{
			return state[i].get(id).value;
		}

		/**
		 * @brief Perform the operation on the specified range
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 */
		void operate(size_t l, size_t r, std::vector<System>& state) const;

		/**
		 * @brief Apply the QFT operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger (inverse QFT) operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;

	};

	/**
	 * @brief Inverse quantum Fourier transform (inverse QFT)
	 * @details Performs the inverse quantum Fourier transform on an integer register
	 */
	struct InverseQFT : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Root of unity omega = e^(2πi/2^n) */
		complex_t omega;

		ClassControllable

		/**
		 * @brief Constructor (name version)
		 * @param reg_ins Register name
		 */
		InverseQFT(std::string_view reg_ins);

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 */
		InverseQFT(size_t reg_in);

		/**
		 * @brief Get the value at the specified position (helper function)
		 * @param i Index
		 * @param state System state vector
		 * @return Reference to the value
		 */
		inline size_t& val(size_t i, std::vector<System>& state) const
		{
			return state[i].get(id).value;
		}

		/**
		 * @brief Perform the operation on the specified range
		 * @param l Left boundary
		 * @param r Right boundary
		 * @param state System state vector
		 */
		void operate(size_t l, size_t r, std::vector<System>& state) const;

		/**
		 * @brief Apply the inverse QFT operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

	};

	// Deprecated alias; see docs/naming_conventions.md. Remove in the next major version.
	using inverseQFT [[deprecated("use InverseQFT")]] = InverseQFT;

	/**
	 * @brief Full quantum Fourier transform
	 * @details Full QFT implementation optimized with the FFT algorithm, including bit-reversal preprocessing
	 */
	struct QFT_Full : BaseOperator {
		using BaseOperator::operator();
		using BaseOperator::dag;

		/** @brief Register ID */
		size_t id;

		/** @brief Number of qubits */
		size_t n_digits;

		/** @brief Root of unity omega = e^(2πi/2^n) */
		complex_t omega;

		/** @brief Threshold */
		const size_t few_threshold = n_digits - 1;

		/** @brief Full state size */
		size_t full_size;

		/** @brief Extra amplitude factor */
		double extra_amplitude;

		/** @brief Bit-reversal table */
		std::vector<uint64_t> bitrev;

		ClassControllable

		/**
		 * @brief Precompute the bit-reversal table
		 */
		inline void precompute_bitrev() {
			profiler _("prefcompute_bitrev");
			if (bitrev.empty())
			{
				bitrev.resize(full_size);
#ifndef SINGLE_THREAD
#pragma omp parallel for schedule(static)
#endif
				for (int64_t i = 0; i < full_size; ++i) {
					uint64_t x = i;
					uint64_t rev = 0;
					for (size_t j = 0; j < n_digits; ++j) {
						rev = (rev << 1) | (x & 1);
						x >>= 1;
					}
					bitrev[i] = rev;
				}
			}
		}

		/**
		 * @brief Constructor (ID version)
		 * @param reg_in Register ID
		 */
		QFT_Full(size_t reg_in)
			: id(reg_in), n_digits(System::size_of(reg_in)), 
			full_size(pow2(n_digits)), extra_amplitude(1.0 / std::sqrt(full_size))
		{
			double theta = 2 * pi / pow2(n_digits);
			omega = complex_t{ cos(theta), sin(theta) };
			precompute_bitrev();
		}

		/**
		 * @brief Constructor (name version)
		 * @param reg_in Register name
		 */
		QFT_Full(std::string_view reg_in)
			: id(System::get(reg_in)), n_digits(System::size_of(id)),
			full_size(pow2(n_digits)), extra_amplitude(1.0 / std::sqrt(full_size))
		{
			double theta = 2 * pi / pow2(n_digits);
			omega = complex_t{ cos(theta), sin(theta) };
			precompute_bitrev();
		}

		/**
		 * @brief Get the value at the specified position (helper function)
		 * @param i Index
		 * @param state System state vector
		 * @return Reference to the value
		 */
		inline size_t& val(size_t i, std::vector<System>& state) const
		{
			return state[i].get(id).value;
		}

		/**
		 * @brief Sparse bucket operation
		 * @param positions Position list
		 * @param state System state vector
		 */
		void operate_bucket_sparse(const std::vector<size_t>& positions, std::vector<System>& state) const;

		/**
		 * @brief FFT implementation
		 * @param positions Position list
		 * @param state System state vector
		 * @param inverse Whether this is the inverse transform
		 */
		void fft(const std::vector<size_t>& positions, std::vector<System>& state, bool inverse) const;

		/**
		 * @brief In-place bucket operation
		 * @param positions Position list
		 * @param state System state vector
		 */
		void operate_bucket_inplace(const std::vector<size_t>& positions, std::vector<System>& state) const;

		/**
		 * @brief In-place inverse bucket operation
		 * @param positions Position list
		 * @param state System state vector
		 */
		void operate_bucket_inplace_inv(const std::vector<size_t>& positions, std::vector<System>& state) const;

		/**
		 * @brief Apply the full QFT operation
		 * @param state System state vector
		 */
		void operator()(std::vector<System>& state) const;

		/**
		 * @brief Apply the dagger (inverse QFT) operation
		 * @param state System state vector
		 */
		void dag(std::vector<System>& state) const;
	};
}
