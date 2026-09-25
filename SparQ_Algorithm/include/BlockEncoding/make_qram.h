/**
 * @file make_qram.h
 * @brief QRAM 数据准备工具（经典侧）
 * @details 提供浮点矩阵/向量到 QRAM 定点补码数据的转换（scaleAndConvertVector 系列）
 *          以及 QRAM 层级树的构造（make_vector_tree）：叶子层父结点存储两子结点
 *          补码值的平方和，其余内部结点存储子结点直接之和，生成的树供
 *          QRAMCircuit_qutrit 的状态制备与块编码条件旋转（Div_Sqrt_Arccos /
 *          CondRot_Fixed_Bool，见 block_encoding_via_QRAM.h）使用
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
	 * @brief 行优先展平矩阵转列优先展平（即矩阵转置）
	 * @param row_vec 行优先展平的 n×n 方阵数据
	 * @return 列优先展平的同份数据
	 * @throws 当输入长度不是完全平方数时抛出异常
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
	 * @brief 缩放并量化为定点补码（std::vector 版本）
	 * @param input_vec 输入浮点数据（展平矩阵或普通向量）
	 * @param exponent 缩放指数（每个元素先乘以 2^exponent）
	 * @param data_size 目标定点位宽
	 * @param from_matrix true 时输入视为行优先展平矩阵并先做列优先转置，
	 *                    false 时视为普通向量直接量化
	 * @return 量化后按 data_size 位补码编码的无符号整数向量
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
	 * @brief 缩放并量化为定点补码（DenseVector 版本，不做转置）
	 * @param input_vec 输入浮点向量
	 * @param exponent 缩放指数（每个元素先乘以 2^exponent）
	 * @param data_size 目标定点位宽
	 * @return 量化后按 data_size 位补码编码的无符号整数向量
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
	 * @brief 缩放并量化为定点补码（DenseMatrix 版本，先转置为列优先）
	 * @param input_vec 输入浮点方阵
	 * @param exponent 缩放指数（每个元素先乘以 2^exponent）
	 * @param data_size 目标定点位宽
	 * @return 列优先展平后量化补码编码的无符号整数向量
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
	 * @brief 由叶子数据自底向上构造 QRAM 层级树
	 * @param dist 叶子层数据（scaleAndConvertVector 输出的补码整数）
	 * @param data_size 定点位宽（叶子层用 get_complement 还原真值）
	 * @return 层序（广度优先）展平的树结点数组：[顶层内部结点, ..., 叶子, 0]，
	 *         其中叶子层的父结点存储两子结点补码真值的平方和（范数平方），
	 *         其余内部结点存储子结点之和，末尾追加 0 作为占位槽
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
						// the leaf nodes，calculated with get_complement
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