/**
 * @file make_qram.h
 * @brief QRAM data preparation utilities (classical side)
 * @details Provides the conversion from floating-point matrices/vectors to QRAM fixed-point
 *          two's-complement data (the scaleAndConvertVector family) as well as the construction
 *          of the QRAM hierarchy tree (make_vector_tree): parents of the leaf layer store the
 *          sum of squares of their two children's two's-complement values, and the remaining
 *          internal nodes store the direct sum of their children. The generated tree is used by
 *          QRAMCircuit_qutrit for state preparation and block encoding conditional rotations
 *          (Div_Sqrt_Arccos / CondRot_Fixed_Bool, see block_encoding_via_QRAM.h)
 */

#pragma once
#include "sparse_state_simulator.h"
#include "qram_circuit_qutrit.h"
#include <iostream>
#include <fstream>
#include "matrix.h"
#include <Eigen/Eigen>
#include <filesystem>

namespace qram_simulator {
	/**
	 * @brief Convert a row-major flattened matrix to column-major flattened form (i.e. transpose
	 *        the matrix)
	 * @param row_vec Data of an n×n square matrix flattened in row-major order
	 * @return The same data flattened in column-major order
	 * @throws Throws an exception if the input length is not a perfect square
	 */
	inline std::vector<double> get_column_flatten(const std::vector<double>& row_vec)
	{
		size_t size = row_vec.size();
		size_t n = static_cast<size_t>(std::sqrt(size));
		if (n * n != size) {
			throw_general_runtime_error();
		}

		std::vector<double> col_vec(size);
		for (size_t i = 0; i < n; ++i) {
			for (size_t j = 0; j < n; ++j) {
				col_vec[j * n + i] = row_vec[i * n + j];
			}
		}
		return col_vec;
	}

	/**
	 * @brief Scale and quantize to fixed-point two's complement (std::vector version)
	 * @param input_vec Input floating-point data (a flattened matrix or a plain vector)
	 * @param exponent Scaling exponent (each element is first multiplied by 2^exponent)
	 * @param data_size Target fixed-point bit width
	 * @param from_matrix When true the input is treated as a row-major flattened matrix and
	 *                    transposed to column-major first; when false it is treated as a plain
	 *                    vector and quantized directly
	 * @return Unsigned integer vector with the quantized values encoded as data_size-bit
	 *         two's complement
	 */
	inline std::vector<uint64_t> scaleAndConvertVector(const std::vector<double>& input_vec, int exponent,
		size_t data_size, bool from_matrix = true)
	{
		std::vector<double> col_vec;
		// matrix transpose
		if (from_matrix) {
			col_vec = get_column_flatten(input_vec);
		}
		else {
			col_vec = input_vec;
		}
		// calculate the scale number
		double scale = std::pow(2.0, exponent);
		std::vector<uint64_t> outputVec;
		outputVec.reserve(col_vec.size());

		for (double value : col_vec) {
			// rescale and convert to unsigned __int64
			size_t scaledValue = make_complement(static_cast<int64_t>(std::llround(value * scale)), data_size);
			outputVec.push_back(scaledValue);
		}

		return outputVec;
	}

	/**
	 * @brief Scale and quantize to fixed-point two's complement (DenseVector version, no transpose)
	 * @param input_vec Input floating-point vector
	 * @param exponent Scaling exponent (each element is first multiplied by 2^exponent)
	 * @param data_size Target fixed-point bit width
	 * @return Unsigned integer vector with the quantized values encoded as data_size-bit
	 *         two's complement
	 */
	inline std::vector<uint64_t> scaleAndConvertVector(const DenseVector<double>& input_vec, int exponent,
		size_t data_size)
	{
		const std::vector<double> &col_vec = input_vec.data;
		double scale = std::pow(2.0, exponent);
		std::vector<uint64_t> outputVec;
		outputVec.reserve(col_vec.size());
		for (double value : col_vec) {
			// rescale and convert to unsigned __int64
			size_t scaledValue = make_complement(static_cast<int64_t>(std::llround(value * scale)), data_size);
			outputVec.push_back(scaledValue);
		}

		return outputVec;
	}

	/**
	 * @brief Scale and quantize to fixed-point two's complement (DenseMatrix version, transposed
	 *        to column-major first)
	 * @param input_vec Input floating-point square matrix
	 * @param exponent Scaling exponent (each element is first multiplied by 2^exponent)
	 * @param data_size Target fixed-point bit width
	 * @return Unsigned integer vector of two's-complement-encoded quantized values after
	 *         column-major flattening
	 */
	inline std::vector<uint64_t> scaleAndConvertVector(const DenseMatrix<double>& input_vec, int exponent,
		size_t data_size)
	{
		std::vector<double> col_vec = get_column_flatten(input_vec.data);
		double scale = std::pow(2.0, exponent);
		std::vector<uint64_t> outputVec;
		outputVec.reserve(col_vec.size());
		for (double value : col_vec) {
			// rescale and convert to unsigned __int64
			size_t scaledValue = make_complement(static_cast<int64_t>(std::llround(value * scale)), data_size);
			outputVec.push_back(scaledValue);
		}

		return outputVec;
	}

	/**
	 * @brief Build the QRAM hierarchy tree bottom-up from the leaf data
	 * @param dist Leaf-layer data (two's-complement integers output by scaleAndConvertVector)
	 * @param data_size Fixed-point bit width (the leaf layer uses get_complement to restore the
	 *                  true values)
	 * @return Tree node array flattened in level (breadth-first) order: [top internal nodes, ...,
	 *         leaves, 0], where the parents of the leaf layer store the sum of squares of their
	 *         two children's two's-complement true values (squared norms), the remaining internal
	 *         nodes store the sum of their children, and a trailing 0 is appended as a placeholder
	 *         slot
	 */
	inline std::vector<uint64_t> make_vector_tree(const std::vector<uint64_t>& dist, size_t data_size) {
		size_t dist_sz = dist.size();
		std::vector<uint64_t> temp_tree = dist;
		std::vector<uint64_t> tree;

		do {
			std::vector<uint64_t> temp;
			temp.reserve(dist_sz / 2);

			for (size_t i = 0; i < dist_sz; i += 2) {
				if (i + 1 < dist_sz) { // avoid overflow
					if (dist_sz == dist.size()) {
						// the leaf nodes, calculated with get_complement
						temp.push_back(
							get_complement(temp_tree[i], data_size) * get_complement(temp_tree[i], data_size) +
							get_complement(temp_tree[i + 1], data_size) * get_complement(temp_tree[i + 1], data_size)
						);
					}
					else {
						// other nodes, sum directly.
						temp.push_back(temp_tree[i] + temp_tree[i + 1]);
					}
				}
			}

			// combine all nodes
			temp.insert(temp.end(), temp_tree.begin(), temp_tree.end());
			temp_tree = std::move(temp);

		} while ((dist_sz = (dist_sz + 1) / 2) > 1); //update dist_sz to match the layers.
		temp_tree.push_back(0);
		tree.insert(tree.end(), temp_tree.begin(), temp_tree.end());
		return tree;
	}
}