#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include <iostream>
#include <fstream>
#include "matrix.h"
#include <Eigen/Eigen>
#include <filesystem>

// #define OPTIMIZE_HADAM
#define OPTIMIZE_HADAM_INT
#define OPTIMIZE_ROT
// #define OPTIMIZE_PREP

/**
 * @file block_encoding.h
 * @brief Common utilities and compile-time switches for block encoding experiments
 * @details Provides formatted printing of complex/real vectors and a helper that zero-pads
 *          result vectors, and centrally defines the implementation optimization switches for
 *          block encoding (OPTIMIZE_HADAM_INT, OPTIMIZE_ROT, etc.). The concrete block encoding
 *          operators live in the BlockEncoding/ subdirectory
 *          (block_encoding_tridiagonal.h, block_encoding_via_QRAM.h, make_qram.h)
 */

namespace qram_simulator {
	/**
	 * @brief Print a complex vector with the given precision
	 * @param zvec Complex vector
	 * @param precision Number of decimal places
	 */
	inline void print_complex_vec(const std::vector<complex_t>& zvec, int precision)
	{
		fmt::print("\n");
		for (auto& z : zvec)
		{
			fmt::print("({:.{}f}, {:.{}f}i)  ", z.real(), precision, z.imag(), precision);
		}
		fmt::print("\n");
	}

	/**
	 * @brief Zero-pad the complex result vector to 4 times the main register space dimension
	 * @param vec Original result vector
	 * @param size Dimension of the main register space
	 * @return New zero-padded vector
	 */
	inline std::vector<complex_t> get_output(const std::vector<complex_t> &vec, int size)
	{
		int dim = 4 * size; //size is the dimensionality of main_reg space.
		std::vector<complex_t> newVec(dim, complex_t(0.0, 0.0));
		std::copy(vec.begin(), vec.end(), newVec.begin());
		return newVec;
	}

	/**
	 * @brief Zero-pad the real result vector to 4 times the main register space dimension
	 *        (real-valued version of get_output)
	 * @param vec Original result vector
	 * @param size Dimension of the main register space
	 * @return New zero-padded vector
	 */
	inline std::vector<double> get_output(const std::vector<double> &vec, int size)
	{
		int dim = 4 * size; //size is the dimensionality of main_reg space.
		std::vector<double> newVec(dim, 0.0);
		std::copy(vec.begin(), vec.end(), newVec.begin());
		return newVec;
	}
}
